# KLV Tag Registry (MISB ST 0601.8)

This table is generated from `data/stanag4609_tags.ini`. That INI file is the authoritative source used by the encoder, decoder, and example scripts.

## How To Use This Table

- `Type` and `Length` determine how values are encoded in the Local Set.
- `Range` and `Scale` define how physical values map to raw integers.
- `Units` describe the physical quantity represented by the tag.
- `Encoding` notes special constraints (string encoding or nested local sets).

## Encoding Rules Summary

- Tags declared as `String` are encoded as ASCII (ISO 646) and truncated to 127 bytes.
- Tags declared as `bytes` are raw byte payloads.
- Tags with `Range` use linear scaling across the provided min/max.
- Tags without `Range` use `Scale` when provided; otherwise raw integer values are used.

## Local Set Tags

The following tags represent nested local sets and should be provided as raw bytes:

- 48 Security Local Set
- 66 Target Location Covariance Matrix
- 73 RVT Local Data Set
- 74 VMTI Local Data Set
- 81 Image Horizon Pixel Pack (Floating Length Pack, not semantically decoded)
- 94 MIIS Core Identifier (ST 1204 Binary Value, not semantically decoded)
- 95 SAR Motion Imagery Metadata (nested ST 1206 Local Set, not semantically decoded)

For JSON input, use `hex:` or `base64:` strings for these tags.

## Table

| ID | Name | Type | Length | Units | Range | Encoding | Scale |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 01 | Checksum | uint16 | 2 |  | 0..(2^16-1) |  |  |
| 02 | UNIX Time Stamp | uint64 | 8 | Microseconds | 0..(2^64-1) |  | 1 microsecond. |
| 03 | Mission ID | String | 127 |  | 1..127 | ISO 646 |  |
| 04 | Platform Tail Number | String | 127 |  | 1..127 | ISO 646 |  |
| 05 | Platform Heading Angle | uint16 | 2 | Degrees | 0..360 |  | ~5.5 milli degrees. |
| 06 | Platform Pitch Angle | int16 | 2 | Degrees | +/- 20 |  | ~610 micro degrees. |
| 07 | Platform Roll Angle | int16 | 2 | Degrees | +/- 50 |  |  |
| 08 | Platform True Airspeed | uint8 | 1 | Meters/Second | 0..255 |  | 1 meter/second. |
| 09 | Platform Indicated Airspeed | uint8 | 1 | Meters/Second | 0..255 |  | 1 meter/second. |
| 10 | Platform Designation | String | 127 |  | 1..127 | ISO 646 |  |
| 11 | Image Source Sensor | String | 127 |  | 1..127 | ISO 646 |  |
| 12 | Image Coordinate System | String | 127 |  | 1..127 | ISO 646 |  |
| 13 | Sensor Latitude | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 14 | Sensor Longitude | int32 | 4 | Degrees | +/- 180 |  | ~84 nano degrees. |
| 15 | Sensor True Altitude | uint16 | 2 | Meters | -900..19000 |  |  |
| 16 | Sensor Horizontal Field of View | uint16 | 2 | Degrees | 0..180 |  |  |
| 17 | Sensor Vertical Field of View | uint16 | 2 | Degrees | 0..180 |  |  |
| 18 | Sensor Relative Azimuth Angle | uint32 | 4 | Degrees | 0..360 |  |  |
| 19 | Sensor Relative Elevation Angle | int32 | 4 | Degrees | +/- 180 |  |  |
| 20 | Sensor Relative Roll Angle | uint32 | 4 | Degrees | 0..360 |  |  |
| 21 | Slant Range | uint32 | 4 | Meters | 0..5000000 |  |  |
| 22 | Target Width | uint16 | 2 | Meters | 0..10000 |  |  |
| 23 | Frame Center Latitude | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 24 | Frame Center Longitude | int32 | 4 | Degrees | +/- 180 |  | ~84 nano degrees. |
| 25 | Frame Center Elevation | uint16 | 2 | Meters | -900..19000 |  |  |
| 26 | Offset Corner Latitude Point 1 | int16 | 2 | Degrees | +/- 0.075 |  |  |
| 27 | Offset Corner Longitude Point 1 | int16 | 2 | Degrees | +/- 0.075 |  |  |
| 28 | Offset Corner Latitude Point 2 | int16 | 2 | Degrees | +/- 0.075 |  |  |
| 29 | Offset Corner Longitude Point 2 | int16 | 2 | Degrees | +/- 0.075 |  |  |
| 30 | Offset Corner Latitude Point 3 | int16 | 2 | Degrees | +/- 0.075 |  |  |
| 31 | Offset Corner Longitude Point 3 | int16 | 2 | Degrees | +/- 0.075 |  |  |
| 32 | Offset Corner Latitude Point 4 | int16 | 2 | Degrees | +/- 0.075 |  |  |
| 33 | Offset Corner Longitude Point 4 | int16 | 2 | Degrees | +/- 0.075 |  |  |
| 34 | Icing Detected | uint8 | 1 | None | 0..255 |  |  |
| 35 | Wind Direction | uint16 | 2 | Degrees | 0..360 |  |  |
| 36 | Wind Speed | uint8 | 1 | Meters/Second | 0..100 |  |  |
| 37 | Static Pressure | uint16 | 2 | Millibars | 0..5000 |  |  |
| 38 | Density Altitude | uint16 | 2 | Meters | -900..19000 |  |  |
| 39 | Outside Air Temperature | int8 | 1 | Celsius | -128..127 |  |  |
| 40 | Target Location Latitude | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 41 | Target Location Longitude | int32 | 4 | Degrees | +/- 180 |  | ~84 nano degrees. |
| 42 | Target Location Elevation | uint16 | 2 | Meters | -900..19000 |  |  |
| 43 | Target Track Gate Width | uint8 | 1 | Pixels | 0..512 |  |  |
| 44 | Target Track Gate Height | uint8 | 1 | Pixels | 0..512 |  |  |
| 45 | Target Error Estimate CE90 | uint16 | 2 | Meters | 0..4095 |  |  |
| 46 | Target Error Estimate LE90 | uint16 | 2 | Meters | 0..4095 |  |  |
| 47 | Generic Flag Data 01 | uint8 | 1 | None | 0..255 |  |  |
| 48 | Security Local Set | bytes | 0 |  |  |  |  |
| 49 | Differential Pressure | uint16 | 2 | Millibars | 0..5000 |  |  |
| 50 | Platform Angle of Attack | int16 | 2 | Degrees | +/- 20 |  |  |
| 51 | Platform Vertical Speed | int16 | 2 | Meters/Second | +/- 180 |  |  |
| 52 | Platform Sideslip Angle | int16 | 2 | Degrees | +/- 20 |  |  |
| 53 | Airfield Barometric Pressure | uint16 | 2 | Millibars | 0..5000 |  |  |
| 54 | Airfield Elevation | uint16 | 2 | Meters | -900..19000 |  |  |
| 55 | Relative Humidity | uint8 | 1 | Percent | 0..100 |  |  |
| 56 | Platform Ground Speed | uint8 | 1 | Meters/Second | 0..255 |  | 1 meter/second. |
| 57 | Ground Range | uint32 | 4 | Meters | 0..5000000 |  |  |
| 58 | Platform Fuel Remaining | uint16 | 2 | Kilograms | 0..65535 |  |  |
| 59 | Platform Call Sign | String | 127 |  | 1..127 | ISO 646 |  |
| 60 | Weapon Load | uint16 | 2 | None | 0..65535 |  |  |
| 61 | Weapon Fired | uint8 | 1 | None | 0..255 |  |  |
| 62 | Laser PRF Code | uint16 | 2 | None | 0..65535 |  |  |
| 63 | Sensor Field of View Name | String | 127 |  | 1..127 | ISO 646 |  |
| 64 | Platform Magnetic Heading | uint16 | 2 | Degrees | 0..360 |  |  |
| 65 | UAS Datalink LS Version Number | uint8 | 1 | None | 0..255 |  |  |
| 66 | Target Location Covariance Matrix | bytes | 0 |  |  |  |  |
| 67 | Alternate Platform Latitude | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 68 | Alternate Platform Longitude | int32 | 4 | Degrees | +/- 180 |  | ~84 nano degrees. |
| 69 | Alternate Platform Altitude | uint16 | 2 | Meters | -900..19000 |  |  |
| 70 | Alternate Platform Name | String | 127 |  | 1..127 | ISO 646 |  |
| 71 | Alternate Platform Heading | uint16 | 2 | Degrees | 0..360 |  |  |
| 72 | Event Start Time | uint64 | 8 | Microseconds | 0..(2^64-1) |  | 1 microsecond. |
| 73 | RVT Local Data Set | bytes | 0 |  |  |  |  |
| 74 | VMTI Local Data Set | bytes | 0 |  |  |  |  |
| 75 | Sensor Ellipsoid Height | uint16 | 2 | Meters | -900..19000 |  |  |
| 76 | Alternate Platform Ellipsoid Height | uint16 | 2 | Meters | -900..19000 |  |  |
| 77 | Operational Mode | uint8 | 1 | None | 0..255 |  |  |
| 78 | Frame Center Height Above Ellipsoid | uint16 | 2 | Meters | -900..19000 |  |  |
| 79 | Sensor North Velocity | int16 | 2 | Meters/Second | +/- 327 |  |  |
| 80 | Sensor East Velocity | int16 | 2 | Meters/Second | +/- 327 |  |  |
| 81 | Image Horizon Pixel Pack | bytes | 0 |  |  | Floating Length Pack (raw) |  |
| 82 | Corner Latitude Point 1 (Full) | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 83 | Corner Longitude Point 1 (Full) | int32 | 4 | Degrees | +/- 180 |  | ~84 nano degrees. |
| 84 | Corner Latitude Point 2 (Full) | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 85 | Corner Longitude Point 2 (Full) | int32 | 4 | Degrees | +/- 180 |  | ~84 nano degrees. |
| 86 | Corner Latitude Point 3 (Full) | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 87 | Corner Longitude Point 3 (Full) | int32 | 4 | Degrees | +/- 180 |  | ~84 nano degrees. |
| 88 | Corner Latitude Point 4 (Full) | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 89 | Corner Longitude Point 4 (Full) | int32 | 4 | Degrees | +/- 180 |  | ~84 nano degrees. |
| 90 | Platform Pitch Angle (Full) | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 91 | Platform Roll Angle (Full) | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 92 | Platform Angle of Attack (Full) | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 93 | Platform Sideslip Angle (Full) | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 94 | MIIS Core Identifier | bytes | 0 |  |  | ST 1204 Binary Value (raw) |  |
| 95 | SAR Motion Imagery Metadata | bytes | 0 |  |  | Nested ST 1206 Local Set (raw) |  |

## Tag Descriptions

Each entry below is derived from `data/stanag4609_tags.ini`.

### 01 Checksum
Type: uint16 (2 bytes). Units: none. Range: 0..(2^16-1).

### 02 UNIX Time Stamp
Type: uint64 (8 bytes). Units: Microseconds. Range: 0..(2^64-1). Scale: 1 microsecond.

### 03 Mission ID
Type: String (max 127 bytes). Units: none. Range: 1..127. Encoding: ISO 646.

### 04 Platform Tail Number
Type: String (max 127 bytes). Units: none. Range: 1..127. Encoding: ISO 646.

### 05 Platform Heading Angle
Type: uint16 (2 bytes). Units: Degrees. Range: 0..360. Scale: ~5.5 milli degrees.

### 06 Platform Pitch Angle
Type: int16 (2 bytes). Units: Degrees. Range: +/- 20. Scale: ~610 micro degrees.

### 07 Platform Roll Angle
Type: int16 (2 bytes). Units: Degrees. Range: +/- 50.

### 08 Platform True Airspeed
Type: uint8 (1 byte). Units: Meters/Second. Range: 0..255. Scale: 1 meter/second.

### 09 Platform Indicated Airspeed
Type: uint8 (1 byte). Units: Meters/Second. Range: 0..255. Scale: 1 meter/second.

### 10 Platform Designation
Type: String (max 127 bytes). Units: none. Range: 1..127. Encoding: ISO 646.

### 11 Image Source Sensor
Type: String (max 127 bytes). Units: none. Range: 1..127. Encoding: ISO 646.

### 12 Image Coordinate System
Type: String (max 127 bytes). Units: none. Range: 1..127. Encoding: ISO 646.

### 13 Sensor Latitude
Type: int32 (4 bytes). Units: Degrees. Range: +/- 90. Scale: ~42 nano degrees.

### 14 Sensor Longitude
Type: int32 (4 bytes). Units: Degrees. Range: +/- 180. Scale: ~84 nano degrees.

### 15 Sensor True Altitude
Type: uint16 (2 bytes). Units: Meters. Range: -900..19000.

### 16 Sensor Horizontal Field of View
Type: uint16 (2 bytes). Units: Degrees. Range: 0..180.

### 17 Sensor Vertical Field of View
Type: uint16 (2 bytes). Units: Degrees. Range: 0..180.

### 18 Sensor Relative Azimuth Angle
Type: uint32 (4 bytes). Units: Degrees. Range: 0..360.

### 19 Sensor Relative Elevation Angle
Type: int32 (4 bytes). Units: Degrees. Range: +/- 180.

### 20 Sensor Relative Roll Angle
Type: uint32 (4 bytes). Units: Degrees. Range: 0..360.

### 21 Slant Range
Type: uint32 (4 bytes). Units: Meters. Range: 0..5000000.

### 22 Target Width
Type: uint16 (2 bytes). Units: Meters. Range: 0..10000.

### 23 Frame Center Latitude
Type: int32 (4 bytes). Units: Degrees. Range: +/- 90. Scale: ~42 nano degrees.

### 24 Frame Center Longitude
Type: int32 (4 bytes). Units: Degrees. Range: +/- 180. Scale: ~84 nano degrees.

### 25 Frame Center Elevation
Type: uint16 (2 bytes). Units: Meters. Range: -900..19000.

### 26 Offset Corner Latitude Point 1
Type: int16 (2 bytes). Units: Degrees. Range: +/- 0.075.

### 27 Offset Corner Longitude Point 1
Type: int16 (2 bytes). Units: Degrees. Range: +/- 0.075.

### 28 Offset Corner Latitude Point 2
Type: int16 (2 bytes). Units: Degrees. Range: +/- 0.075.

### 29 Offset Corner Longitude Point 2
Type: int16 (2 bytes). Units: Degrees. Range: +/- 0.075.

### 30 Offset Corner Latitude Point 3
Type: int16 (2 bytes). Units: Degrees. Range: +/- 0.075.

### 31 Offset Corner Longitude Point 3
Type: int16 (2 bytes). Units: Degrees. Range: +/- 0.075.

### 32 Offset Corner Latitude Point 4
Type: int16 (2 bytes). Units: Degrees. Range: +/- 0.075.

### 33 Offset Corner Longitude Point 4
Type: int16 (2 bytes). Units: Degrees. Range: +/- 0.075.

### 34 Icing Detected
Type: uint8 (1 byte). Units: none. Range: 0..255.

### 35 Wind Direction
Type: uint16 (2 bytes). Units: Degrees. Range: 0..360.

### 36 Wind Speed
Type: uint8 (1 byte). Units: Meters/Second. Range: 0..100.

### 37 Static Pressure
Type: uint16 (2 bytes). Units: Millibars. Range: 0..5000.

### 38 Density Altitude
Type: uint16 (2 bytes). Units: Meters. Range: -900..19000.

### 39 Outside Air Temperature
Type: int8 (1 byte). Units: Celsius. Range: -128..127.

### 40 Target Location Latitude
Type: int32 (4 bytes). Units: Degrees. Range: +/- 90. Scale: ~42 nano degrees.

### 41 Target Location Longitude
Type: int32 (4 bytes). Units: Degrees. Range: +/- 180. Scale: ~84 nano degrees.

### 42 Target Location Elevation
Type: uint16 (2 bytes). Units: Meters. Range: -900..19000.

### 43 Target Track Gate Width
Type: uint8 (1 byte). Units: Pixels. Range: 0..512.

### 44 Target Track Gate Height
Type: uint8 (1 byte). Units: Pixels. Range: 0..512.

### 45 Target Error Estimate CE90
Type: uint16 (2 bytes). Units: Meters. Range: 0..4095.

### 46 Target Error Estimate LE90
Type: uint16 (2 bytes). Units: Meters. Range: 0..4095.

### 47 Generic Flag Data 01
Type: uint8 (1 byte). Units: none. Range: 0..255.

### 48 Security Local Set
Type: bytes (variable length). Units: none. Value: raw bytes; supply hex or base64 when using JSON.

### 49 Differential Pressure
Type: uint16 (2 bytes). Units: Millibars. Range: 0..5000.

### 50 Platform Angle of Attack
Type: int16 (2 bytes). Units: Degrees. Range: +/- 20.

### 51 Platform Vertical Speed
Type: int16 (2 bytes). Units: Meters/Second. Range: +/- 180.

### 52 Platform Sideslip Angle
Type: int16 (2 bytes). Units: Degrees. Range: +/- 20.

### 53 Airfield Barometric Pressure
Type: uint16 (2 bytes). Units: Millibars. Range: 0..5000.

### 54 Airfield Elevation
Type: uint16 (2 bytes). Units: Meters. Range: -900..19000.

### 55 Relative Humidity
Type: uint8 (1 byte). Units: Percent. Range: 0..100.

### 56 Platform Ground Speed
Type: uint8 (1 byte). Units: Meters/Second. Range: 0..255. Scale: 1 meter/second.

### 57 Ground Range
Type: uint32 (4 bytes). Units: Meters. Range: 0..5000000.

### 58 Platform Fuel Remaining
Type: uint16 (2 bytes). Units: Kilograms. Range: 0..65535.

### 59 Platform Call Sign
Type: String (max 127 bytes). Units: none. Range: 1..127. Encoding: ISO 646.

### 60 Weapon Load
Type: uint16 (2 bytes). Units: none. Range: 0..65535.

### 61 Weapon Fired
Type: uint8 (1 byte). Units: none. Range: 0..255.

### 62 Laser PRF Code
Type: uint16 (2 bytes). Units: none. Range: 0..65535.

### 63 Sensor Field of View Name
Type: String (max 127 bytes). Units: none. Range: 1..127. Encoding: ISO 646.

### 64 Platform Magnetic Heading
Type: uint16 (2 bytes). Units: Degrees. Range: 0..360.

### 65 UAS Datalink LS Version Number
Type: uint8 (1 byte). Units: none. Range: 0..255.

### 66 Target Location Covariance Matrix
Type: bytes (variable length). Units: none. Value: raw bytes; supply hex or base64 when using JSON.

### 67 Alternate Platform Latitude
Type: int32 (4 bytes). Units: Degrees. Range: +/- 90. Scale: ~42 nano degrees.

### 68 Alternate Platform Longitude
Type: int32 (4 bytes). Units: Degrees. Range: +/- 180. Scale: ~84 nano degrees.

### 69 Alternate Platform Altitude
Type: uint16 (2 bytes). Units: Meters. Range: -900..19000.

### 70 Alternate Platform Name
Type: String (max 127 bytes). Units: none. Range: 1..127. Encoding: ISO 646.

### 71 Alternate Platform Heading
Type: uint16 (2 bytes). Units: Degrees. Range: 0..360.

### 72 Event Start Time
Type: uint64 (8 bytes). Units: Microseconds. Range: 0..(2^64-1). Scale: 1 microsecond.

### 73 RVT Local Data Set
Type: bytes (variable length). Units: none. Value: raw bytes; supply hex or base64 when using JSON.

### 74 VMTI Local Data Set
Type: bytes (variable length). Units: none. Value: raw bytes; supply hex or base64 when using JSON.

### 75 Sensor Ellipsoid Height
Type: uint16 (2 bytes). Units: Meters. Range: -900..19000.

### 76 Alternate Platform Ellipsoid Height
Type: uint16 (2 bytes). Units: Meters. Range: -900..19000.

### 77 Operational Mode
Type: uint8 (1 byte). Units: none. Range: 0..255.

### 78 Frame Center Height Above Ellipsoid
Type: uint16 (2 bytes). Units: Meters. Range: -900..19000.

### 79 Sensor North Velocity
Type: int16 (2 bytes). Units: Meters/Second. Range: +/- 327. Out-of-range indicator: -(2^15) = 0x8000. Resolution: ~1 cm/sec.

### 80 Sensor East Velocity
Type: int16 (2 bytes). Units: Meters/Second. Range: +/- 327. Out-of-range indicator: -(2^15) = 0x8000. Resolution: ~1 cm/sec.

### 81 Image Horizon Pixel Pack
Type: bytes (variable length, MISB RP 0701 Floating Length Pack). Units: none.
Structure per ST 0601.8 Section 8.81: Start x0, Start y0, End x1, End y1
(each Uint8, percent 0..100 — mandatory), optionally followed by Start
Latitude, Start Longitude, End Latitude, End Longitude (each Int32,
+/-90 / +/-180 degrees). gstklvplugin does not decode this internal
structure; the value is preserved byte-exact. Supply as `hex:` or
`base64:` in JSON.

### 82 Corner Latitude Point 1 (Full)
Type: int32 (4 bytes). Units: Degrees. Range: +/- 90. Scale: ~42 nano degrees. Error indicator: -(2^31) = 0x80000000. Frame Latitude for upper left corner (WGS84).

### 83 Corner Longitude Point 1 (Full)
Type: int32 (4 bytes). Units: Degrees. Range: +/- 180. Scale: ~84 nano degrees. Error indicator: -(2^31) = 0x80000000. Frame Longitude for upper left corner (WGS84).

### 84 Corner Latitude Point 2 (Full)
Type: int32 (4 bytes). Units: Degrees. Range: +/- 90. Scale: ~42 nano degrees. Error indicator: -(2^31) = 0x80000000. Frame Latitude for upper right corner (WGS84).

### 85 Corner Longitude Point 2 (Full)
Type: int32 (4 bytes). Units: Degrees. Range: +/- 180. Scale: ~84 nano degrees. Error indicator: -(2^31) = 0x80000000. Frame Longitude for upper right corner (WGS84).

### 86 Corner Latitude Point 3 (Full)
Type: int32 (4 bytes). Units: Degrees. Range: +/- 90. Scale: ~42 nano degrees. Error indicator: -(2^31) = 0x80000000. Frame Latitude for lower right corner (WGS84).

### 87 Corner Longitude Point 3 (Full)
Type: int32 (4 bytes). Units: Degrees. Range: +/- 180. Scale: ~84 nano degrees. Error indicator: -(2^31) = 0x80000000. Frame Longitude for lower right corner (WGS84).

### 88 Corner Latitude Point 4 (Full)
Type: int32 (4 bytes). Units: Degrees. Range: +/- 90. Scale: ~42 nano degrees. Error indicator: -(2^31) = 0x80000000. Frame Latitude for lower left corner (WGS84).

### 89 Corner Longitude Point 4 (Full)
Type: int32 (4 bytes). Units: Degrees. Range: +/- 180. Scale: ~84 nano degrees. Error indicator: -(2^31) = 0x80000000. Frame Longitude for lower left corner (WGS84).

### 90 Platform Pitch Angle (Full)
Type: int32 (4 bytes). Units: Degrees. Range: +/- 90. Scale: ~42 nano degrees. Out-of-range indicator: -(2^31) = 0x80000000.

### 91 Platform Roll Angle (Full)
Type: int32 (4 bytes). Units: Degrees. Range: +/- 90. Scale: ~42 nano degrees. Error indicator: -(2^31) = 0x80000000.

### 92 Platform Angle of Attack (Full)
Type: int32 (4 bytes). Units: Degrees. Range: +/- 90. Scale: ~42 nano degrees. Out-of-range indicator: -(2^31) = 0x80000000.

### 93 Platform Sideslip Angle (Full)
Type: int32 (4 bytes). Units: Degrees. Range: +/- 90. Scale: ~42 nano degrees. Out-of-range indicator: -(2^31) = 0x80000000.
**Standard ambiguity:** ST 0601.8 Section 8.93's summary header lists Range
as "+/- 180" for this tag, but the same section's Notes/Conversion Formula
state "Map -(2^31-1)..(2^31-1) to +/-90" — identical to Tags 90-92.
gstklvplugin implements +/-90 for consistency with Tags 90-92 and this
tag's own mapping notes. This is a documented inconsistency in the
normative source, not a project decision to override the standard.

### 94 MIIS Core Identifier
Type: bytes (variable length). Units: none. References MISB ST 1204; carries
only the ST1204 Binary Value (no ST1204 key/length). gstklvplugin does not
semantically decode ST1204; the value is preserved byte-exact. Supply as
`hex:` or `base64:` in JSON.

### 95 SAR Motion Imagery Metadata
Type: bytes (variable length). Units: none. References MISB ST 1206; carries
a nested SAR Motion Imagery Metadata Local Set. gstklvplugin does not
semantically decode ST1206; the nested payload is preserved byte-exact.
Supply as `hex:` or `base64:` in JSON.

## Updating the Registry

To change tags or add new definitions:

- Edit `data/stanag4609_tags.ini`.
- Regenerate this table with the helper workflow used in this repo.
- Re-run validation to ensure scaling and types remain consistent.

## Notes

- Local set tags are encoded as raw bytes in this project. Use `hex:` or `base64:` payloads to supply their values.
- JSON keys are numeric strings, for example: `"13": 40.71`.


## Author

Author: Mouhsine Kassimi Farhaoui  
Mail: mouhsine98@gmail.com
