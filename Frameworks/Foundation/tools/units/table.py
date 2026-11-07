# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Dimension class, base unit, then (name, symbol, coefficient[, constant]).
# Coefficients: the base unit's value of one unit (physical definitions).
TABLE = [
 ("NSUnitAcceleration", "metersPerSecondSquared", [
  ("metersPerSecondSquared", "m/s²", 1), ("gravity", "g", 9.81)]),
 ("NSUnitAngle", "degrees", [
  ("degrees", "°", 1), ("arcMinutes", "ʹ", 1/60), ("arcSeconds", "ʺ", 1/3600),
  ("radians", "rad", 57.2957795131), ("gradians", "grad", 0.9), ("revolutions", "rev", 360)]),
 ("NSUnitArea", "squareMeters", [
  ("squareMegameters", "Mm²", 1e12), ("squareKilometers", "km²", 1e6), ("squareMeters", "m²", 1),
  ("squareCentimeters", "cm²", 1e-4), ("squareMillimeters", "mm²", 1e-6), ("squareMicrometers", "µm²", 1e-12),
  ("squareNanometers", "nm²", 1e-18), ("squareInches", "in²", 0.00064516), ("squareFeet", "ft²", 0.09290304),
  ("squareYards", "yd²", 0.83612736), ("squareMiles", "mi²", 2589988.110336), ("acres", "ac", 4046.8564224),
  ("ares", "a", 100), ("hectares", "ha", 10000)]),
 ("NSUnitConcentrationMass", "gramsPerLiter", [
  ("gramsPerLiter", "g/L", 1), ("milligramsPerDeciliter", "mg/dL", 0.01)]),
 ("NSUnitDispersion", "partsPerMillion", [("partsPerMillion", "ppm", 1)]),
 ("NSUnitDuration", "seconds", [
  ("hours", "hr", 3600), ("minutes", "min", 60), ("seconds", "s", 1), ("milliseconds", "ms", 1e-3),
  ("microseconds", "µs", 1e-6), ("nanoseconds", "ns", 1e-9), ("picoseconds", "ps", 1e-12)]),
 ("NSUnitElectricCharge", "coulombs", [
  ("coulombs", "C", 1), ("megaampereHours", "MAh", 3.6e9), ("kiloampereHours", "kAh", 3.6e6),
  ("ampereHours", "Ah", 3600), ("milliampereHours", "mAh", 3.6), ("microampereHours", "µAh", 0.0036)]),
 ("NSUnitElectricCurrent", "amperes", [
  ("megaamperes", "MA", 1e6), ("kiloamperes", "kA", 1e3), ("amperes", "A", 1),
  ("milliamperes", "mA", 1e-3), ("microamperes", "µA", 1e-6)]),
 ("NSUnitElectricPotentialDifference", "volts", [
  ("megavolts", "MV", 1e6), ("kilovolts", "kV", 1e3), ("volts", "V", 1),
  ("millivolts", "mV", 1e-3), ("microvolts", "µV", 1e-6)]),
 ("NSUnitElectricResistance", "ohms", [
  ("megaohms", "MΩ", 1e6), ("kiloohms", "kΩ", 1e3), ("ohms", "Ω", 1),
  ("milliohms", "mΩ", 1e-3), ("microohms", "µΩ", 1e-6)]),
 ("NSUnitEnergy", "joules", [
  ("kilojoules", "kJ", 1000), ("joules", "J", 1), ("kilocalories", "kCal", 4184),
  ("calories", "cal", 4.184), ("kilowattHours", "kWh", 3.6e6)]),
 ("NSUnitFrequency", "hertz", [
  ("terahertz", "THz", 1e12), ("gigahertz", "GHz", 1e9), ("megahertz", "MHz", 1e6), ("kilohertz", "kHz", 1e3),
  ("hertz", "Hz", 1), ("millihertz", "mHz", 1e-3), ("microhertz", "µHz", 1e-6), ("nanohertz", "nHz", 1e-9),
  ("framesPerSecond", "FPS", 1)]),
 ("NSUnitFuelEfficiency", "litersPer100Kilometers", [
  ("litersPer100Kilometers", "L/100km", 1), ("milesPerImperialGallon", "mpg", 282.480936, None, "reciprocal"),
  ("milesPerGallon", "mpg", 235.214583, None, "reciprocal")]),
 ("NSUnitIlluminance", "lux", [("lux", "lx", 1)]),
 ("NSUnitInformationStorage", "bits", [
  ("bytes", "B", 8), ("bits", "bit", 1), ("nibbles", "nibble", 4),
  ("yottabytes", "YB", 8e24), ("zettabytes", "ZB", 8e21), ("exabytes", "EB", 8e18), ("petabytes", "PB", 8e15),
  ("terabytes", "TB", 8e12), ("gigabytes", "GB", 8e9), ("megabytes", "MB", 8e6), ("kilobytes", "kB", 8e3),
  ("yottabits", "Yb", 1e24), ("zettabits", "Zb", 1e21), ("exabits", "Eb", 1e18), ("petabits", "Pb", 1e15),
  ("terabits", "Tb", 1e12), ("gigabits", "Gb", 1e9), ("megabits", "Mb", 1e6), ("kilobits", "kb", 1e3),
  ("yobibytes", "YiB", 8 * 1024**8), ("zebibytes", "ZiB", 8 * 1024**7), ("exbibytes", "EiB", 8 * 1024**6),
  ("pebibytes", "PiB", 8 * 1024**5), ("tebibytes", "TiB", 8 * 1024**4), ("gibibytes", "GiB", 8 * 1024**3),
  ("mebibytes", "MiB", 8 * 1024**2), ("kibibytes", "KiB", 8 * 1024),
  ("yobibits", "Yib", 1024**8), ("zebibits", "Zib", 1024**7), ("exbibits", "Eib", 1024**6),
  ("pebibits", "Pib", 1024**5), ("tebibits", "Tib", 1024**4), ("gibibits", "Gib", 1024**3),
  ("mebibits", "Mib", 1024**2), ("kibibits", "Kib", 1024)]),
 ("NSUnitLength", "meters", [
  ("megameters", "Mm", 1e6), ("kilometers", "km", 1e3), ("hectometers", "hm", 100), ("decameters", "dam", 10),
  ("meters", "m", 1), ("decimeters", "dm", 0.1), ("centimeters", "cm", 0.01), ("millimeters", "mm", 0.001),
  ("micrometers", "µm", 1e-6), ("nanometers", "nm", 1e-9), ("picometers", "pm", 1e-12), ("inches", "in", 0.0254),
  ("feet", "ft", 0.3048), ("yards", "yd", 0.9144), ("miles", "mi", 1609.344), ("scandinavianMiles", "smi", 10000),
  ("lightyears", "ly", 9.4607304725808e15), ("nauticalMiles", "NM", 1852), ("fathoms", "ftm", 1.8288),
  ("furlongs", "fur", 201.168), ("astronomicalUnits", "ua", 1.495978707e11), ("parsecs", "pc", 3.0856775814913673e16)]),
 ("NSUnitMass", "kilograms", [
  ("kilograms", "kg", 1), ("grams", "g", 1e-3), ("decigrams", "dg", 1e-4), ("centigrams", "cg", 1e-5),
  ("milligrams", "mg", 1e-6), ("micrograms", "µg", 1e-9), ("nanograms", "ng", 1e-12), ("picograms", "pg", 1e-15),
  ("ounces", "oz", 0.028349523125), ("poundsMass", "lb", 0.45359237), ("stones", "st", 6.35029318),
  ("metricTons", "t", 1000), ("shortTons", "ton", 907.18474), ("carats", "ct", 0.0002),
  ("ouncesTroy", "oz t", 0.0311034768), ("slugs", "slug", 14.593903)]),
 ("NSUnitPower", "watts", [
  ("terawatts", "TW", 1e12), ("gigawatts", "GW", 1e9), ("megawatts", "MW", 1e6), ("kilowatts", "kW", 1e3),
  ("watts", "W", 1), ("milliwatts", "mW", 1e-3), ("microwatts", "µW", 1e-6), ("nanowatts", "nW", 1e-9),
  ("picowatts", "pW", 1e-12), ("femtowatts", "fW", 1e-15), ("horsepower", "hp", 745.69987158227022)]),
 ("NSUnitPressure", "newtonsPerMetersSquared", [
  ("newtonsPerMetersSquared", "N/m²", 1), ("gigapascals", "GPa", 1e9), ("megapascals", "MPa", 1e6),
  ("kilopascals", "kPa", 1e3), ("hectopascals", "hPa", 100), ("inchesOfMercury", "inHg", 3386.389),
  ("bars", "bar", 1e5), ("millibars", "mbar", 100), ("millimetersOfMercury", "mmHg", 133.322387415),
  ("poundsForcePerSquareInch", "psi", 6894.757293168)]),
 ("NSUnitSpeed", "metersPerSecond", [
  ("metersPerSecond", "m/s", 1), ("kilometersPerHour", "km/h", 1 / 3.6), ("milesPerHour", "mph", 0.44704),
  ("knots", "kn", 1852 / 3600)]),
 ("NSUnitTemperature", "kelvin", [
  ("kelvin", "K", 1), ("celsius", "°C", 1, 273.15), ("fahrenheit", "°F", 5 / 9, 255.37222222222222)]),
 ("NSUnitVolume", "liters", [
  ("megaliters", "ML", 1e6), ("kiloliters", "kL", 1e3), ("liters", "L", 1), ("deciliters", "dL", 0.1),
  ("centiliters", "cL", 0.01), ("milliliters", "mL", 0.001), ("cubicKilometers", "km³", 1e12),
  ("cubicMeters", "m³", 1000), ("cubicDecimeters", "dm³", 1), ("cubicCentimeters", "cm³", 0.001),
  ("cubicMillimeters", "mm³", 1e-6), ("cubicInches", "in³", 0.016387064), ("cubicFeet", "ft³", 28.316846592),
  ("cubicYards", "yd³", 764.554857984), ("cubicMiles", "mi³", 4.16818182544058e12), ("acreFeet", "af", 1233481.83754752),
  ("bushels", "bsh", 35.23907016688), ("teaspoons", "tsp", 0.00492892159375), ("tablespoons", "tbsp", 0.01478676478125),
  ("fluidOunces", "fl oz", 0.0295735295625), ("cups", "cup", 0.24), ("pints", "pt", 0.473176473),
  ("quarts", "qt", 0.946352946), ("gallons", "gal", 3.785411784), ("imperialTeaspoons", "tsp", 0.00591938802083),
  ("imperialTablespoons", "tbsp", 0.0177581640625), ("imperialFluidOunces", "fl oz", 0.0284130625),
  ("imperialPints", "pt", 0.56826125), ("imperialQuarts", "qt", 1.1365225), ("imperialGallons", "gal", 4.54609),
  ("metricCups", "metric cup", 0.25)]),
]
