"""Bounds-checked reader for 16-bit Windows NE executables (read-only).

Parses segments, per-segment relocations, imported module names and the
resident/non-resident export name tables. No code is executed.
"""

import struct


class NEError(ValueError):
    pass


class NE:
    def __init__(self, data, ne_offset=None, segments=True):
        self.data = data
        if ne_offset is None:
            if data[:2] != b"MZ":
                raise NEError("missing MZ signature")
            ne_offset = self._u("<I", 0x3C)[0]
        if data[ne_offset:ne_offset + 2] != b"NE":
            raise NEError("missing NE signature")
        self.ne = ne_offset
        w = self.word
        self.entry = (w(0x16), w(0x14))
        self.align = w(0x32)
        if w(0x1C) > 255 or w(0x1E) > 64 or self.align > 16:
            raise NEError("implausible NE header")
        self.modules = []
        for i in range(w(0x1E)):
            at = self.ne + w(0x2A) + self._u("<H", self.ne + w(0x28) + i * 2)[0]
            self.modules.append(self._pstring(at))
        self.segments = []
        for i in range(w(0x1C) if segments else 0):
            sector, length, flags, minimum = self._u("<4H", self.ne + w(0x22) + i * 8)
            offset = sector << self.align
            size = length or (0x10000 if sector else 0)
            seg = {"number": i + 1, "offset": offset, "size": size, "flags": flags,
                   "data": bool(flags & 1), "relocs": []}
            if sector and flags & 0x100:
                seg["relocs"] = self._relocs(offset + size)
            self.segments.append(seg)
        self.names = {}
        self._names(self.ne + w(0x26), stop=self.ne + w(0x28))
        if self._u("<I", self.ne + 0x2C)[0]:
            self._names(self._u("<I", self.ne + 0x2C)[0], limit=w(0x20))

    def word(self, rel):
        return self._u("<H", self.ne + rel)[0]

    def _u(self, fmt, at):
        if at < 0 or at + struct.calcsize(fmt) > len(self.data):
            raise NEError(f"truncated at {at:#x}")
        return struct.unpack_from(fmt, self.data, at)

    def _pstring(self, at):
        n = self._u("<B", at)[0]
        return self._u(f"<{n}s", at + 1)[0].decode("latin-1")

    def _names(self, at, stop=None, limit=None):
        end = at + limit if limit else len(self.data)
        first = True
        while at < end and (stop is None or at < stop):
            n = self._u("<B", at)[0]
            if n == 0:
                break
            name = self._pstring(at)
            ordinal = self._u("<H", at + 1 + n)[0]
            if not first:  # first entry is the module name/description
                self.names.setdefault(ordinal, name)
            first = False
            at += 3 + n

    def _relocs(self, at):
        count = self._u("<H", at)[0]
        out = []
        for i in range(count):
            src, flags, offset, a, b = self._u("<BBHHH", at + 2 + i * 8)
            kind = flags & 3
            if kind == 0:
                target = ("internal", a & 0xFF, b)
            elif kind == 1:
                target = ("ordinal", self.modules[a - 1] if 0 < a <= len(self.modules) else f"?{a}", b)
            elif kind == 2:
                mod = self.modules[a - 1] if 0 < a <= len(self.modules) else f"?{a}"
                target = ("name", mod, self._pstring(self.ne + self.word(0x2A) + b))
            else:
                target = ("osfixup", a, b)
            out.append({"src": src, "additive": bool(flags & 4), "offset": offset, "target": target})
        return out

    def segment_bytes(self, number):
        seg = self.segments[number - 1]
        return self.data[seg["offset"]:seg["offset"] + seg["size"]]

    def fixup_sites(self, number):
        """Yield (site_offset, reloc) for every patched location, following chains."""
        code = self.segment_bytes(number)
        for r in self.segments[number - 1]["relocs"]:
            if r["additive"]:
                yield r["offset"], r
                continue
            site, seen = r["offset"], set()
            while site != 0xFFFF and site not in seen and site + 2 <= len(code):
                seen.add(site)
                yield site, r
                site = struct.unpack_from("<H", code, site)[0]


def find_embedded_ne(data):
    """Locate an NE header inside a file (e.g. a Wine *.dll16 builtin)."""
    try:
        return NE(data, segments=False)
    except NEError:
        pass
    at = data.find(b"NE")
    while at != -1:
        try:
            ne = NE(data, at, segments=False)
            if ne.names:
                return ne
        except (NEError, struct.error, IndexError):
            pass
        at = data.find(b"NE", at + 1)
    raise NEError("no NE header found")
