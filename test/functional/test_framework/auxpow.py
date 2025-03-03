#!/usr/bin/env python3
# Copyright (c) 2014-2018 Daniel Kraft
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Basic code for working with auxpow.  This is used for the regtests (e.g. from
# auxpow_testing.py), but also for contrib/auxpow/getwork-wrapper.py.

import binascii
import codecs
import hashlib

def constructAuxpow(block):
  """
  Starts to construct a minimal auxpow, ready to be mined.  Returns the
  coinbase tx (with one OP_RETURN output containing a Syscoin-like commitment)
  and the unmined parent block header as hex strings.

  This differs minimally from the old version: we only add a vout with a single
  OP_RETURN to hold "SYSCOIN" + 32 zero bytes + 4 zero bytes.
  """

  # Convert the block from ASCII to bytes for hex manipulation
  block = codecs.encode(block, 'ascii')

  # Original partial coinbase script for merged mining:
  # "fa be 'm' 'm'"
  coinbase = b"fabe" + binascii.hexlify(b"m" * 2)
  coinbase += block
  coinbase += b"01000000" + (b"00" * 4)

  # Construct "vector" of transaction inputs as before
  vin = b"01"
  vin += (b"00" * 32) + (b"ff" * 4)
  # scriptSig length
  vin += codecs.encode("%02x" % (len(coinbase) // 2), "ascii") + coinbase
  vin += (b"ff" * 4)

  # -------------------------------
  # Here is the minimal difference:
  # We add exactly 1 output, with an OP_RETURN script containing
  # "SYSCOIN" + 32 zero bytes + 4 zero bytes.
  # Everything else remains the same.
  # -------------------------------

  # Vout count (1)
  vout = b"01"

  # Output 0: amount = 0 (8 bytes, little-endian)
  vout += b"0000000000000000"

  # Now build the scriptPubKey for OP_RETURN
  #  - OP_RETURN = 0x6a
  #  - We push <data> as a single chunk if it's <0x4c in length
  # Data is 7 bytes "SYSCOIN", plus 32 zero bytes for the hash, plus 4 zero bytes for height
  syscoinTag = b"737973636f696e"        # hex for "syscoin"
  dataPayload = syscoinTag + block
  dataPayloadLen = len(dataPayload) // 2   # each byte is 2 hex chars
  # OP_RETURN + <push-len> + <payload>
  scriptPubKey = b"6a" + ("%02x" % dataPayloadLen).encode("ascii") + dataPayload

  # scriptPubKey length in bytes
  scriptPubKeyLen = len(scriptPubKey) // 2
  vout += codecs.encode("%02x" % scriptPubKeyLen, "ascii") + scriptPubKey

  # Build the full coinbase tx (version=01000000, vin, vout, locktime=00000000)
  tx = b"01000000" + vin + vout + b"00000000"
  txHash = doubleHashHex(tx)

  # Construct the parent block header
  header  = b"01000000"
  header += b"00" * 32
  # The coinbase is the only tx, so merkle root is the coinbase txid reversed
  header += reverseHex(txHash)
  header += b"00" * 4
  header += b"00" * 4
  header += b"00" * 4

  return (tx.decode("ascii"), header.decode("ascii"))

def finishAuxpow (tx, header):
  """
  Constructs the finished auxpow hex string based on the mined header.
  """

  blockhash = doubleHashHex (header)

  # Build the MerkleTx part of the auxpow.
  auxpow = codecs.encode (tx, 'ascii')
  auxpow += blockhash
  auxpow += b"00"
  auxpow += b"00" * 4

  # Extend to full auxpow.
  auxpow += b"00"
  auxpow += b"00" * 4
  auxpow += header

  return auxpow.decode ("ascii")

def doubleHashHex (data):
  """
  Perform Bitcoin's Double-SHA256 hash on the given hex string.
  """

  hasher = hashlib.sha256 ()
  hasher.update (binascii.unhexlify (data))
  data = hasher.digest ()

  hasher = hashlib.sha256 ()
  hasher.update (data)

  return reverseHex (hasher.hexdigest ())

def reverseHex (data):
  """
  Flip byte order in the given data (hex string).
  """

  b = bytearray (binascii.unhexlify (data))
  b.reverse ()

  return binascii.hexlify (b)

def getworkByteswap (data):
  """
  Run the byte-order swapping step necessary for working with getwork.
  """

  data = bytearray (data)
  assert len (data) % 4 == 0
  for i in range (0, len (data), 4):
    data[i], data[i + 3] = data[i + 3], data[i]
    data[i + 1], data[i + 2] = data[i + 2], data[i + 1]

  return data
