// Read-only check of Address Library IDs (format 5 dense, or format 2 compressed) against the target PE file.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const [exePath, addressPath] = process.argv.slice(2);
if (!exePath || !addressPath) throw new Error('Usage: node Verify-Runtime.mjs SkyrimSE.exe versionlib-1-7-104-0.bin|versionlib-1-6-1170-0.bin');
const pe = fs.readFileSync(exePath), lib = fs.readFileSync(addressPath);
const format = lib.readUInt32LE(0);
assert([2, 5].includes(format), `Address Library format ${format}`);
const runtime = [4,8,12,16].map(o => lib.readUInt32LE(o)).join('.');
// Each supported runtime has its own ClearSight.dll build; the AE IDs below are shared.
assert(['1.7.104.0', '1.6.1170.0'].includes(runtime), `Unsupported runtime ${runtime}`);
console.log(JSON.stringify({runtime, format}));
let count, resolve;
if (format === 5) {
  // v13 and later: dense u32 RVA table indexed by ID.
  count = lib.readUInt32LE(92);
  assert.equal(lib.length, 96 + count * 4, 'Dense address table size');
  resolve = id => lib.readUInt32LE(96 + id * 4);
} else {
  // v11 and earlier AE files: delta-encoded (id, offset) pairs, as unpacked by CommonLib's IDDatabase.
  let p = 24 + lib.readUInt32LE(20);
  const pointerSize = lib.readUInt32LE(p), total = lib.readUInt32LE(p + 4); p += 8;
  const u8 = () => lib.readUInt8(p++), u16 = () => (p += 2, lib.readUInt16LE(p - 2));
  const u32 = () => (p += 4, lib.readUInt32LE(p - 4)), u64 = () => (p += 8, Number(lib.readBigUInt64LE(p - 8)));
  const next = (mode, prev) => [() => u64(), () => prev + 1, () => prev + u8(), () => prev - u8(),
    () => prev + u16(), () => prev - u16(), () => u16(), () => u32()][mode]();
  const offsets = new Map();
  let id = 0, offset = 0, maxId = 0;
  for (let i = 0; i < total; ++i) {
    const type = u8(), lo = type & 0xf, hi = type >> 4;
    assert(lo < 8, 'Address Library ID encoding');
    id = next(lo, id);
    const scaled = hi & 8;
    offset = next(hi & 7, scaled ? Math.floor(offset / pointerSize) : offset);
    if (scaled) offset *= pointerSize;
    offsets.set(id, offset); if (id > maxId) maxId = id;
  }
  assert(p <= lib.length, 'Compressed address table within file');
  count = maxId + 1;
  resolve = id => offsets.get(id) ?? 0;
}
const nt = pe.readUInt32LE(0x3c);
assert.equal(pe.toString('ascii', nt, nt + 4), 'PE\0\0');
const sectionsCount = pe.readUInt16LE(nt + 6);
const optional = nt + 24, imageBase = Number(pe.readBigUInt64LE(optional + 24));
const sectionStart = optional + pe.readUInt16LE(nt + 20);
const sections = Array.from({length: sectionsCount}, (_, i) => {
  const p = sectionStart + i * 40;
  return {name: pe.toString('ascii', p, p + 8).replace(/\0/g, ''),
    size: pe.readUInt32LE(p + 8), rva: pe.readUInt32LE(p + 12),
    rawSize: pe.readUInt32LE(p + 16), raw: pe.readUInt32LE(p + 20), flags: pe.readUInt32LE(p + 36)};
});
function section(rva) { return sections.find(s => rva >= s.rva && rva < s.rva + Math.max(s.size, s.rawSize)); }
function fileOffset(rva) {
  const s = section(rva); assert(s && rva - s.rva < s.rawSize, 'RVA has file backing');
  return s.raw + rva - s.rva;
}
for (const [name, id, slots] of [
  ['Lighting',255053,[4,6,7]], ['Grass',254719,[6,7]], ['DistantTree',255223,[6,7]],
  ['Effect',255194,[6,7]], ['Particle',255421,[6,7]], ['BloodSplatter',254785,[6,7]]
]) {
  assert(id < count, `${name} ID in range`);
  const rva = resolve(id);
  assert(rva !== 0, `${name} ID exists`);
  const entries = slots.map(slot => {
    const fn = Number(pe.readBigUInt64LE(fileOffset(rva + slot * 8))) - imageBase;
    assert(section(fn)?.flags & 0x20000000, `${name} slot ${slot} points to executable code`);
    return {slot, rva: `0x${fn.toString(16)}`};
  });
  console.log(JSON.stringify({name, table: `0x${rva.toString(16)}`, entries}));
}
const initCall = resolve(77226) + 0x2bc;
const initOffset = fileOffset(initCall);
assert.equal(pe[initOffset], 0xe8, 'Renderer init hook is a relative call');
const initTarget = initCall + 5 + pe.readInt32LE(initOffset + 1);
assert(section(initTarget)?.flags & 0x20000000, 'Renderer init call target is executable');
console.log(JSON.stringify({name:'RendererInit', call:`0x${initCall.toString(16)}`, target:`0x${initTarget.toString(16)}`}));
for (const [name, id] of [['QuestTarget.GetTargetRef',25284], ['QuestTarget.GetTrackingRef',25285]]) {
  assert(id < count, `${name} ID in range`);
  const rva = resolve(id);
  assert(rva && (section(rva)?.flags & 0x20000000), `${name} resolves to executable code`);
  console.log(JSON.stringify({name, rva: `0x${rva.toString(16)}`}));
}
console.log(`PASS (${runtime}): target version and shader vtable entries resolve in the installed executable. This does not validate runtime behavior.`);
