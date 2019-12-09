// WError represents an error from a wasm module
//
export class WError extends Error {
  constructor(code, message, file, line) {
    super(message, file || "wasm", line || 0)
    this.name = "WError"
    this.code = code
  }
}

// Get & clear last WErr. Returns null if there was no error.
// Uses a descriptive name so to help in stack traces.
export function error_from_wasm() { // :WError|null
  let code = _WErrGetCode()
  if (code != 0) {
    let msgptr = _WErrGetMsg()
    let message = msgptr != 0 ? UTF8ArrayToString(HEAPU8, msgptr) : ""
    _WErrClear()
    return new WError(code, message)
  }
}

export function werrCheck() {
  let err = error_from_wasm()
  if (err) {
    throw err
  }
}

// bytebuf takes an ArrayBuffer or Iterable<byte> and returns a Uint8Array
//
// bytebuf(buf :ArrayBuffer|Iterable<byte>|byte[]) : Uint8Array
//
export function bytebuf(buf) {
  if (buf instanceof Uint8Array) {
    return buf
  }
  return new Uint8Array(buf)
}

// mallocbuf allocates memory in the WASM heap and copies length bytes
// from byteArray into the allocated location.
// Returns the address to the allocated memory.
//
export function mallocbuf(byteArray, length) {
  const offs = _wrealloc(0, length)
  HEAPU8.set(byteArray, offs)
  return offs
}

// malloc32 allocates at least size bytes on 32-bit boundary.
// Returns two values: original_address and aligned_address.
// You should call free() with original_address.
//
export function malloc32(size) {
  let ptr_orig = _wrealloc(0, size + 3)
  return [ptr_orig, ptr_orig + (4 - (ptr_orig % 4))]
}

// malloc16 allocates at least size bytes on 16-bit boundary.
// Returns two values: original_address and aligned_address.
// You should call free() with original_address.
//
export function malloc16(size) {
  let ptr_orig = _wrealloc(0, size + 1)
  return [ptr_orig, ptr_orig + (ptr_orig % 2)]
}

// free wasm heap memory
export function free(ptr) {
  _wfree(ptr)
}


// writeUTF16Str writes str as UTF16 to address ptr.
// ptr must be aligned on a 16-bit boundary.
//
export function writeUTF16Str(str, ptr) {
  for (let i = 0; i < str.length; ++i) {
    HEAP16[ptr >> 1] = str.charCodeAt(i)
    ptr += 2
  }
}


// withTmpBytePtr takes an ArrayBuffer or Uint8Array and:
// 1. copies it into the WASM module memory
// 2. calls fn(pointer, size)
// 3. calls free(pointer)
//
export function withTmpBytePtr(buf, fn) {
  const u8buf = bytebuf(buf)
  const size = u8buf.length
  const ptr = mallocbuf(u8buf, size)
  const r = fn(ptr, size)
  free(ptr)
  return r
}


// withUTF16Str takes a JavaScript string and:
// 1. copies it into the WASM module memory as UTF16, 16-bit aligned
// 2. calls fn(aligned_pointer, bytesize)
// 3. calls free(original_pointer)
//
export function withUTF16Str(str, fn) {
  let bytesize = str.length * 2
  let ptr = _wrealloc(0, bytesize + 1) // +1 for alignment
  let aligned_ptr = (ptr % 2 != 0) ? ptr + 1 : ptr
  writeUTF16Str(str, aligned_ptr, bytesize)
  let r = fn(aligned_ptr, bytesize)
  free(ptr)
  return r
}


export function cstrlen(ptr) {
  let end = ptr >> 0
  while (HEAP8[end]) { end++ }
  return end - ptr
}


// asciicstr interprets memory in buf at offset as an ASCII-encoded string,
// and returns a JavaScript string.
//
export function asciicstr(buf, offset) {
  let str = ''
  while (true) {
    let b = buf[offset++ >> 0]
    if (b == 0) { break }
    str += String.fromCharCode(b)
  }
  return str
}

// cstrStack allocates a UTF-8 encoded version of str as a nul-terminated
// "C string" on the stack.
//
export function cstrStack(str) {
  var ret = 0
  if (str !== null && str !== undefined && str !== 0) {
    var len = (str.length << 2) + 1
    ret = stackAlloc(len)
    stringToUTF8Array(str, HEAPU8, ret, len)
  }
  return ret
}

// used by strFromUTF8Ptr as a temporary address-sized integer
let tmpPtr = 0

Module.postRun.push(() => {
  tmpPtr = _wrealloc(0, 4)
})


// strFromUTF8Ptr provides a pointer-sized integer that can be written
// to by fn. fn is expected to return the number of bytes written to the
// address pointed to by p. The address is dereferenced and the written
// number of bytes are interpreted as UTF8, returning a JS string.
//
// This is useful for efficiently converting UTF8 strings that are
// already allocated inside the library to JavaScript strings.
//
// Example:
//   strFromUTF8Ptr((p)=> _FooGetName(ptr, p))
//   ...
//   u32 EXPORT FooGetName(Foo* f, const char** p) {
//     *p = f->name_ptr;
//     return f->name_len;
//   }
//
// Synopsis:
//   strFromUTF8Ptr( fn :(p:int)=>int )
//
export function strFromUTF8Ptr(fn) {
  let z = fn(tmpPtr)
  let offs = HEAP32[tmpPtr >> 2]
  return z == 0 ? "" : utf8.decode(HEAPU8.subarray(offs, offs + z))
}

// withOutPtr facilitates the following:
//
// 1. calls fn with an address to memory that fits a pointer.
//    fn(outptr) is expected to:
//    a. Write some data into heap memory
//    b. Write the address of that data at outptr (i.e. *outptr = heapaddr)
//    c. Return the length of data written
//
// 2. withOutPtr reads the address from outptr
//    a. If the address is 0 (NULL), returns null
//    b. Else a slice of the heap memory is created, starting at *outptr
//       and ending at ((*outptr) + length_returned_by_fn).
//       A free() function is added to the buffer and it is returned.
//
// It is important to free() the memory of the returned buffer when the caller is done.
// This is implementation specific, so this function can not help you with that.
//
// The return type is as follows:
//   interface HeapData extends Uint8Array {
//     readonly heapAddr :number  // address in heap == *outptr
//   }
//
// Example:
//
//   // WASM module, in C:
//   typedef struct Color_ { char r, g, b; } Color;
//   size_t newColor(const Color** outp) {
//     Color* c = (Color*)malloc(sizeof(Color));
//     c->r = 0xFF;
//     c->g = 0xCA;
//     c->b = 0x0;
//     *outp = c;
//     return sizeof(Color);
//   }
//   void freeColor(const Color* p) {
//     free(p);
//   }
//
//   // JavaScript
//   let color = withOutPtr(_newColor)
//   console.log("RGB:", color[0], color[1], color[2])
//   _freeColor(color.heapAddr)
//
export function withOutPtr(fn) {
  let len = fn(tmpPtr)
  let addr = HEAP32[tmpPtr >> 2]
  if (addr == 0) {
    return null
  }
  let buf = HEAPU8.subarray(addr, addr + len)
  buf.heapAddr = addr
  return buf
}



// withStackFrame saves the stack and calls fn; code in fn can then
// allocate stack memory. When fn returns or throws, the stack is restored
// to the point before this function was called.
// Returns the return value of fn.
//
export function withStackFrame(fn) {
  let stack = stackSave()
  try {
    return fn()
  } finally {
    stackRestore(stack)
  }
}


// ureadU16 reads a (little endian) unsigned 16-bit integer from buf at addr
//
export function ureadU16(buf, addr) {
  return ((buf[addr] | (buf[addr + 1] << 8))) >>> 0
}

// ureadI16 reads a (little endian) signed 16-bit integer from buf at addr
//
export function ureadI16(buf, addr) {
  let n = ((buf[addr]) | (buf[addr + 1] << 8))
  return n >= 0x8000 ? n - 0x10000 : n
}

// ureadU32 reads a (little endian) unsigned 32-bit integer from buf at addr
//
export function ureadU32(buf, addr) {
  return (
    (buf[addr + 3] << 24) |
    (buf[addr + 2] << 16) |
    (buf[addr + 1] << 8) |
    (buf[addr] >>> 0)
  ) >>> 0
}

// ureadU32 reads a (little endian) signed 32-bit integer from buf at addr
//
export function ureadI32(buf, addr) {
  return (
    (buf[addr + 3] << 24) |
    (buf[addr + 2] << 16) |
    (buf[addr + 1] << 8) |
    (buf[addr] >>> 0)
  )
}


// export function ureadI16be(buf, addr) {
//   let n = ((buf[addr] << 8) | (buf[addr + 1]))
//   return n >= 0x8000 ? n - 0x10000 : n
// }

// export function ureadU16be(buf, addr) {
//   return ((buf[addr] << 8) | (buf[addr + 1])) >>> 0
// }

// export function ureadU32be(buf, addr) {
//   return (
//     (buf[addr] << 24) |
//     (buf[addr + 1] << 16) |
//     (buf[addr + 2] << 8) |
//     (buf[addr + 3])
//   ) >>> 0
// }

// export function ureadI32be(buf, addr) {
//   return (
//     (buf[addr] << 24) |
//     (buf[addr + 1] << 16) |
//     (buf[addr + 2] << 8) |
//     (buf[addr + 3])
//   )
// }


// asciiStrToU32 converts a <=4 character string to a u32.
// For example, string -> hb_tag_t
//
export function asciiStrToU32(s) {
  // Note: Should match #define HB_TAG(c1,c2,c3,c4) in hb-common.h
  return (
    ((s.charCodeAt(0) >>> 0) << 24) >>> 0 | // "">>> 0" u32 please
    ((s.charCodeAt(1) >>> 0) << 16) |
    ((s.charCodeAt(2) >>> 0) << 8) |
     (s.charCodeAt(3) >>> 0)
  )
}

export const hbtag = asciiStrToU32

// u32ToAsciiStr converts a u32 to a ASCII string
// For example, hb_tag_t -> string
//
export function u32ToAsciiStr(u) {
  return String.fromCharCode(
    ((u >> 24) & 0xff),
    ((u >> 16) & 0xff),
    ((u >>  8) & 0xff),
    ((u >>  0) & 0xff)
  )
}

// interface utf8 {
//   encode(s :string) :Uint8Array
//   decode(b :Uint8Array) :string
// }
export const utf8 = typeof TextEncoder != 'undefined' ? (() => {
  // Modern browsers
  const enc = new TextEncoder("utf-8")
  const dec = new TextDecoder("utf-8")
  return {
    encode: s => enc.encode(s),
    decode: b => dec.decode(b),
  };
})() : typeof Buffer != 'undefined' ? {
  // Nodejs
  encode: s => new Uint8Array(Buffer.from(s, 'utf-8')),
  decode: b =>
    Buffer.from(b.buffer, b.byteOffset, b.byteLength).toString('utf8'),
} : {
  // Some other pesky JS environment
  encode: s => {
    let asciiBytes = [];
    for (let i = 0, L = s.length; i != L; ++i) {
      asciiBytes[i] = 0xff & s.charCodeAt(i);
    }
    return new Uint8Array(asciiBytes);
  },
  decode: b => String(b),
}


// Converts between 16.16 fixed-point number and 64-bit floating-point numbers
export function fixedToFloat(i) {
  return i / 65536.0
}
export function floatToFixed(f) {
  return (f * 65536.0) >> 0
}

