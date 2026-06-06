Natural Text International
==========================

A watchface for the [Pebble][] that displays the exact time in natural
spoken language — "twenty four past seven", "eight o'clock",
"twelve to two" — using the large, elegant fonts and staggered
animations of the original [PebbleTextWatch][] by Mihai Dumitrache.

[Pebble]: https://getpebble.com/
[PebbleTextWatch]: https://github.com/wearewip/PebbleTextWatch

Inspired by [Mattias Bäcklund's Swedish fuzzy text watch][Swedish fuzzy text
watch], this version extends the concept to ten languages and replaces
fuzzy five-minute rounding with exact spoken minutes.

[Swedish fuzzy text watch]: https://github.com/Sarastro72/Swedish-Fuzzy-Text-watch

Features:

 - Exact time in natural spoken language ("twenty four past seven")
 - "Past" and "to" phrasing, matching how people actually say the time
 - The large and easy-to-read fonts of the original Text Watch
 - Staggered slide animation between updates
 - Between one and four lines of text, depending on length
 - Shorter phrases share a single line (e.g. "one to" / **eight**)

The following options can be configured using the Pebble app on your
phone:

- Invert colors (white-on-black or black-on-white)
- Text alignment (centered, left, or right)
- Language
- Font size
- Date display (shown on tap, with configurable timeout)

Supported languages and example phrases:

| Language   | Past example          | To example         | On the hour     |
|------------|-----------------------|--------------------|-----------------|
| English    | twenty four past seven | twelve to two     | eight o'clock   |
| German     | zwanzig vier nach sieben | zwölf vor zwei  | acht Uhr        |
| French     | vingt quatre passé sept | douze avant deux | huit heures    |
| Spanish    | siete y veinte cuatro | dos menos doce    | siete en punto  |
| Norwegian  | tjue fire over sju    | tolv på to         | åtte            |
| Swedish    | tjugo fyra över sju   | tolv i två         | åtta            |
| Dutch      | twintig vier over zeven | twaalf voor twee | zeven uur      |
| Catalan    | set i vint quatre     | dos menys dotze    | set en punt     |
| Portuguese | sete e vinte quatro   | duas menos doze    | sete em ponto   |


Authors
-------

Thanks to all of the people who made this watchface possible:

- [Mihai Dumitrache][Mihai], implemented an open source version of Text Watch
- [Mattias Bäcklund][Mattias], created Swedish fuzzy text watch
- [Jesse Hallett][Jesse], added configuration options and multiple language support
- [Filip Horvei][iFlips], provided Norwegian translation
- Tomi De Lucca, discovered fix for a severe iOS bug & assisted with Spanish translation

[Mihai]: https://github.com/mmdumi
[Mattias]: https://github.com/Sarastro72
[Jesse]: https://github.com/hallettj
[iFlips]: https://github.com/iFlips


Contributing
------------

If you would like to request a translation, report an error, or suggest
improvements, please [open an issue][issue].

[issue]: https://github.com/adamboutcher/Sliding-Text-International-PT2/issues/new

Pull requests are welcome.
