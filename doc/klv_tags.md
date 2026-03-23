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
- 92 MIIS Core Identifier
- 93 SAR Motion Imagery Local Set

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
| 12 | Image Coordinate System | uint8 | 1 | None | 0..255 |  |  |
| 13 | Sensor Latitude | int32 | 4 | Degrees | +/- 90 |  | ~42 nano degrees. |
| 14 | Sensor Longitude | int32 | 4 | Degrees | +/- 180 |  | ~84 nano degrees. |
| 15 | Sensor True Altitude | uint16 | 2 | Meters | -900..19000 |  |  |
| 16 | Sensor Horizontal Field of View | uint16 | 2 | Degrees | 0..180 |  |  |
| 17 | Sensor Vertical Field of View | uint16 | 2 | Degrees | 0..180 |  |  |
| 18 | Sensor Relative Azimuth Angle | uint32 | 4 | Degrees | 0..360 |  |  |
| 19 | Sensor Relative Elevation Angle | int32 | 4 | Degrees | +/- 180 |  |  |
| 20 | Sensor Relative Roll Angle | int32 | 4 | Degrees | +/- 180 |  |  |
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
| 43 | Target Track Gate Width | uint16 | 2 | Meters | 0..10000 |  |  |
| 44 | Target Track Gate Height | uint16 | 2 | Meters | 0..10000 |  |  |
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
| 79 | Sensor North Velocity | int16 | 2 | Meters/Second | +/- 327.67 |  |  |
| 80 | Sensor East Velocity | int16 | 2 | Meters/Second | +/- 327.67 |  |  |
| 81 | Sensor Up Velocity | int16 | 2 | Meters/Second | +/- 327.67 |  |  |
| 82 | Platform North Velocity | int16 | 2 | Meters/Second | +/- 327.67 |  |  |
| 83 | Platform East Velocity | int16 | 2 | Meters/Second | +/- 327.67 |  |  |
| 84 | Platform Up Velocity | int16 | 2 | Meters/Second | +/- 327.67 |  |  |
| 85 | Alternate Platform North Velocity | int16 | 2 | Meters/Second | +/- 327.67 |  |  |
| 86 | Alternate Platform East Velocity | int16 | 2 | Meters/Second | +/- 327.67 |  |  |
| 87 | Alternate Platform Up Velocity | int16 | 2 | Meters/Second | +/- 327.67 |  |  |
| 88 | Platform Pitch Angle Full | int32 | 4 | Degrees | +/- 20 |  |  |
| 89 | Platform Roll Angle Full | int32 | 4 | Degrees | +/- 50 |  |  |
| 90 | Platform Angle of Attack Full | int32 | 4 | Degrees | +/- 20 |  |  |
| 91 | Platform Sideslip Angle Full | int32 | 4 | Degrees | +/- 20 |  |  |
| 92 | MIIS Core Identifier | bytes | 0 |  |  |  |  |
| 93 | SAR Motion Imagery Local Set | bytes | 0 |  |  |  |  |

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
Type: uint8 (1 byte). Units: none. Range: 0..255.

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
Type: int32 (4 bytes). Units: Degrees. Range: +/- 180.

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
Type: uint16 (2 bytes). Units: Meters. Range: 0..10000.

### 44 Target Track Gate Height
Type: uint16 (2 bytes). Units: Meters. Range: 0..10000.

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
Type: int16 (2 bytes). Units: Meters/Second. Range: +/- 327.67.

### 80 Sensor East Velocity
Type: int16 (2 bytes). Units: Meters/Second. Range: +/- 327.67.

### 81 Sensor Up Velocity
Type: int16 (2 bytes). Units: Meters/Second. Range: +/- 327.67.

### 82 Platform North Velocity
Type: int16 (2 bytes). Units: Meters/Second. Range: +/- 327.67.

### 83 Platform East Velocity
Type: int16 (2 bytes). Units: Meters/Second. Range: +/- 327.67.

### 84 Platform Up Velocity
Type: int16 (2 bytes). Units: Meters/Second. Range: +/- 327.67.

### 85 Alternate Platform North Velocity
Type: int16 (2 bytes). Units: Meters/Second. Range: +/- 327.67.

### 86 Alternate Platform East Velocity
Type: int16 (2 bytes). Units: Meters/Second. Range: +/- 327.67.

### 87 Alternate Platform Up Velocity
Type: int16 (2 bytes). Units: Meters/Second. Range: +/- 327.67.

### 88 Platform Pitch Angle Full
Type: int32 (4 bytes). Units: Degrees. Range: +/- 20.

### 89 Platform Roll Angle Full
Type: int32 (4 bytes). Units: Degrees. Range: +/- 50.

### 90 Platform Angle of Attack Full
Type: int32 (4 bytes). Units: Degrees. Range: +/- 20.

### 91 Platform Sideslip Angle Full
Type: int32 (4 bytes). Units: Degrees. Range: +/- 20.

### 92 MIIS Core Identifier
Type: bytes (variable length). Units: none. Value: raw bytes; supply hex or base64 when using JSON.

### 93 SAR Motion Imagery Local Set
Type: bytes (variable length). Units: none. Value: raw bytes; supply hex or base64 when using JSON.

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
