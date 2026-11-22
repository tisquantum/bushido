# Numbers and units

The game has no floating point.

## Formats

| Quantity | Format | Where it shows |
| --- | --- | --- |
| Angle | 4096 to the turn, kept in 12 bits (`& 0xFFF`); 1024 is a right angle | `SceneNode.rotation`, poses, camera angles, light directions (`ANGLE_FULL`, `ANGLE_HALF`, `ANGLE_QUARTER`, `ANGLE_EIGHTH`, `ANGLE_MASK` in `include/angle.h`) |
| Fraction | Q12: 4096 is 1.0 (`ONE`) | Matrix entries, light colours, HSV values, scales such as `statureScale`, fade and blend progress |
| World position | 32-bit integer units; Y points down, so a floor normal straight up is (0, -4096, 0) | `VECTOR`, `WorldVector`, `MATRIX.t` |
| Model vertex | 16-bit units in the model's space | `SVECTOR`, the 6-byte packed vertices |
| Cloth point | world units times 128 | `ClothPoint` (`include/cloth.h`) |
| Screen position | 16-bit x and y packed in a word, y in the high half; centre (320, 120) | the projected vertex buffers on the scratchpad |
| Screen depth | 16-bit, as the GTE produces it | `SZ`, `OTZ` |
| Colour | 8 bits a channel for packets and ambient colours; Q12 for the light colour matrix | `CVECTOR`, `LightSlot` |
| Sound volume | 0 to 127 | `QueueSoundEffect` |
| Disc position | BCD minute, second, sector | `CdlLOC` in `gCdFileTable` |

The stage grid is 32 by 32 cells of 3,200 units, from -51,200 to +51,200 on X and Z ([stages.md](stages.md)).

A `MATRIX` is the library's: nine Q12 rotation entries and a 32-bit translation. Rotations are built from three
angles by `gRotMatrixBuilders[order]`; the first, `BuildRotationMatrixFast` (`src/render.s`), is the one most
nodes use, and the others are in `src/rot_matrix.c`.

## Trigonometry

- **Sine.** `gSinLutQ12` (`src/scene_math.c`), 4,097 entries: the Q12 sine of `i` turns/4096, truncated, the last
  entry closing the turn. The cosine is the entry 1024 further on (`COS`, `src/rot_matrix.c`). The library's
  `rsin` and `rcos` are also called, in the lights and the cloth wind.
- **Arccosine.** `gAcosLutQ11`, 2,050 entries: the angle whose cosine is `i/2048`, from 1024 down to 0.
  `ComputeAcosQ12` takes a Q12 ratio and indexes it at half; ratios above 1.0 give 0. Two more helpers,
  `ComputeAcosRatio` and `ComputeAcosComplementRatio`, index the table with a Q12 ratio directly and are never
  called.
- **Arctangent** is the library's `ratan2` throughout.
- The Japanese build also has packed sine and cosine routines (`GetPackedSinCos` and its kin in
  `src/render.s`) that nothing calls.

## Square roots

Three tables, each indexed after the GTE's leading-zero counter (LZCS and LZCR) has told how far to shift the
value so that its top bits fit the table:

| Table | Holds | Used by |
| --- | --- | --- |
| `gSqrtLutQ3` (`src/cloth.c`), 1,024 bytes | floor(8 sqrt(i)) | Written out inline wherever it is used (the cameras, positional sound, the cloth solver): values below 1,024 look up directly; larger ones are shifted right by an even count to 10 bits, looked up, and shifted back by half that count |
| `gSqrtLutQ9` (`src/render.s`), 256 halfwords | sqrt(i) times 512 | `ComputeVectorLength3D`: SQR squares the three components, and the sum goes through the same scheme with 8-bit indices |
| `gRecipSqrtLutQ12` (`src/render.s`), 192 halfwords | 32768 / sqrt(64 + i) | `NormalizeGteIrVector`: scales the vector in IR1 to IR3 to length 4096 |

`ComputeVectorLengthSq3D` gives the squared length alone. The cloth also calls the library's `SquareRoot12`.

## Depth to ordering-table entry

The ordering table has 4,101 entries and a drawn primitive takes one from 0 to 4095. Every drawer uses the same
compression (`CompressDepthToOtz`, `src/otz.c`, and the same instructions inline in `src/render.s`). The input
is the screen depth divided by 8 (the drawers' coordinate shift, [rendering.md](rendering.md), is undone on
the way):

    band  = d >> 11                   (d: screen depth / 8)
    entry = (((d & 0x7FF) | ~0xFFF) >> band) & 0xFFF

So depths are cut into bands of 2,048, and each band gets half the entries of the band before it:

| Screen depth | d | Entries |
| --- | --- | --- |
| 0 to 16,383 | 0 to 2,047 | 0 to 2,047, one per unit of d |
| 16,384 to 32,767 | 2,048 to 4,095 | 2,048 to 3,071 |
| 32,768 to 49,151 | 4,096 to 6,143 | 3,072 to 3,583 |
| 49,152 to 65,535 | 6,144 to 8,191 | 3,584 to 3,839 |
| ... | ... | ... halving each band, toward 4095 |

Near the camera the order is exact to eight units of depth; far away many depths share one entry. Entries 4096
to 4100 take no primitive; the fog's final palette move sits at 4091 ([lighting.md](lighting.md)).

`GetDepthScaleShift` (`src/otz.c`) maps a distance to a shift of 0 to 3 at 0x5A00, 0xB500 and 0x16A09, each
about the square root of two times a power of two. `CompressDepthToOtzScratch` takes its shift from the
scratchpad; in the Japanese build its bands are 12 bits wide instead of 11.

## Blending

- `InterpolateOrthonormalMatrix` (`src/render.s`) blends two rotations by a Q12 weight and makes the result
  orthonormal again; pose blending uses it ([models.md](models.md)).
- `LerpSvector` and `LerpMatrix3x3` do the same without the correction; nothing calls them.
- Colours fade with the library's `LoadAverageShort12` (the point light, `src/point_light.c`).
- `ConvertHsvToRgbQ12` and `ConvertRgbToHsvQ12` (`src/stage_light.c`) work in Q12 for all three components.
