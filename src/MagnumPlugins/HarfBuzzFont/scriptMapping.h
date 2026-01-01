/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
              Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/* Used to distinguish which HB_SCRIPT_* values are supported and which not,
   plus to verify that the Magnum and HarfBuzz FourCC values match. Order is
   matching the hb_script_t enum to have additions in a certain version grouped
   together.

   Apart from HB_SCRIPT_INVALID there's 176 entries in HarfBuzz 11.5.0, the
   count of _c() values should match here too, guarded by the both-directional
   switch cases in the test. */
#if defined(_c) && (defined(_c_include_supported) || defined(_c_include_all))
#if _c_include_all || _c_include_supported == 1
/* Unspecified omitted */
_c(Common, COMMON)
_c(Inherited, INHERITED)
_c(Unknown, UNKNOWN)
_c(Arabic, ARABIC)
_c(Armenian, ARMENIAN)
_c(Bengali, BENGALI)
_c(Cyrillic, CYRILLIC)
_c(Devanagari, DEVANAGARI)
_c(Georgian, GEORGIAN)
_c(Greek, GREEK)
_c(Gujarati, GUJARATI)
_c(Gurmukhi, GURMUKHI)
_c(Hangul, HANGUL)
_c(Han, HAN)
_c(Hebrew, HEBREW)
_c(Hiragana, HIRAGANA)
_c(Kannada, KANNADA)
_c(Katakana, KATAKANA)
_c(Lao, LAO)
_c(Latin, LATIN)
_c(Malayalam, MALAYALAM)
_c(Oriya, ORIYA)
_c(Tamil, TAMIL)
_c(Telugu, TELUGU)
_c(Thai, THAI)
_c(Tibetan, TIBETAN)
_c(Bopomofo, BOPOMOFO)
_c(Braille, BRAILLE)
/* This one doesn't match, the Script enum is prefering the Unicode name */
_c(CanadianAboriginal, CANADIAN_SYLLABICS)
_c(Cherokee, CHEROKEE)
_c(Ethiopic, ETHIOPIC)
_c(Khmer, KHMER)
_c(Mongolian, MONGOLIAN)
_c(Myanmar, MYANMAR)
_c(Ogham, OGHAM)
_c(Runic, RUNIC)
_c(Sinhala, SINHALA)
_c(Syriac, SYRIAC)
_c(Thaana, THAANA)
_c(Yi, YI)
_c(Deseret, DESERET)
_c(Gothic, GOTHIC)
_c(OldItalic, OLD_ITALIC)
_c(Buhid, BUHID)
_c(Hanunoo, HANUNOO)
_c(Tagalog, TAGALOG)
_c(Tagbanwa, TAGBANWA)
_c(Cypriot, CYPRIOT)
_c(Limbu, LIMBU)
_c(LinearB, LINEAR_B)
_c(Osmanya, OSMANYA)
_c(Shavian, SHAVIAN)
_c(TaiLe, TAI_LE)
_c(Ugaritic, UGARITIC)
_c(Buginese, BUGINESE)
_c(Coptic, COPTIC)
_c(Glagolitic, GLAGOLITIC)
_c(Kharoshthi, KHAROSHTHI)
_c(NewTaiLue, NEW_TAI_LUE)
_c(OldPersian, OLD_PERSIAN)
_c(SylotiNagri, SYLOTI_NAGRI)
_c(Tifinagh, TIFINAGH)
_c(Balinese, BALINESE)
_c(Cuneiform, CUNEIFORM)
_c(NKo, NKO)
_c(PhagsPa, PHAGS_PA)
_c(Phoenician, PHOENICIAN)
_c(Carian, CARIAN)
_c(Cham, CHAM)
_c(KayahLi, KAYAH_LI)
_c(Lepcha, LEPCHA)
_c(Lycian, LYCIAN)
_c(Lydian, LYDIAN)
_c(OlChiki, OL_CHIKI)
_c(Rejang, REJANG)
_c(Saurashtra, SAURASHTRA)
_c(Sundanese, SUNDANESE)
_c(Vai, VAI)
_c(Avestan, AVESTAN)
_c(Bamum, BAMUM)
_c(EgyptianHieroglyphs, EGYPTIAN_HIEROGLYPHS)
_c(ImperialAramaic, IMPERIAL_ARAMAIC)
_c(InscriptionalPahlavi, INSCRIPTIONAL_PAHLAVI)
_c(InscriptionalParthian, INSCRIPTIONAL_PARTHIAN)
_c(Javanese, JAVANESE)
_c(Kaithi, KAITHI)
_c(Lisu, LISU)
_c(MeeteiMayek, MEETEI_MAYEK)
_c(OldSouthArabian, OLD_SOUTH_ARABIAN)
_c(OldTurkic, OLD_TURKIC)
_c(Samaritan, SAMARITAN)
_c(TaiTham, TAI_THAM)
_c(TaiViet, TAI_VIET)
_c(Batak, BATAK)
_c(Brahmi, BRAHMI)
_c(Mandaic, MANDAIC)
_c(Chakma, CHAKMA)
_c(MeroiticCursive, MEROITIC_CURSIVE)
_c(MeroiticHieroglyphs, MEROITIC_HIEROGLYPHS)
_c(Miao, MIAO)
_c(Sharada, SHARADA)
_c(SoraSompeng, SORA_SOMPENG)
_c(Takri, TAKRI)
#endif
#if _c_include_all || _c_include_supported == HB_VERSION_ATLEAST(0, 9, 30)
_c(BassaVah, BASSA_VAH)
_c(CaucasianAlbanian, CAUCASIAN_ALBANIAN)
_c(Duployan, DUPLOYAN)
_c(Elbasan, ELBASAN)
_c(Grantha, GRANTHA)
_c(Khojki, KHOJKI)
_c(Khudawadi, KHUDAWADI)
_c(LinearA, LINEAR_A)
_c(Mahajani, MAHAJANI)
_c(Manichaean, MANICHAEAN)
_c(MendeKikakui, MENDE_KIKAKUI)
_c(Modi, MODI)
_c(Mro, MRO)
_c(Nabataean, NABATAEAN)
_c(OldNorthArabian, OLD_NORTH_ARABIAN)
_c(OldPermic, OLD_PERMIC)
_c(PahawhHmong, PAHAWH_HMONG)
_c(Palmyrene, PALMYRENE)
_c(PauCinHau, PAU_CIN_HAU)
_c(PsalterPahlavi, PSALTER_PAHLAVI)
_c(Siddham, SIDDHAM)
_c(Tirhuta, TIRHUTA)
_c(WarangCiti, WARANG_CITI)
_c(Ahom, AHOM)
_c(AnatolianHieroglyphs, ANATOLIAN_HIEROGLYPHS)
_c(Hatran, HATRAN)
_c(Multani, MULTANI)
_c(OldHungarian, OLD_HUNGARIAN)
/* It's actually really named SignWriting, single word, in CamelCase */
_c(SignWriting, SIGNWRITING)
#endif
#if _c_include_all || _c_include_supported == HB_VERSION_ATLEAST(1, 3, 0)
_c(Adlam, ADLAM)
_c(Bhaiksuki, BHAIKSUKI)
_c(Marchen, MARCHEN)
_c(Osage, OSAGE)
_c(Tangut, TANGUT)
_c(Newa, NEWA)
#endif
#if _c_include_all || _c_include_supported == HB_VERSION_ATLEAST(1, 6, 0)
_c(MasaramGondi, MASARAM_GONDI)
_c(Nushu, NUSHU)
_c(Soyombo, SOYOMBO)
_c(ZanabazarSquare, ZANABAZAR_SQUARE)
#endif
#if _c_include_all || _c_include_supported == HB_VERSION_ATLEAST(1, 8, 0)
_c(Dogra, DOGRA)
_c(GunjalaGondi, GUNJALA_GONDI)
_c(HanifiRohingya, HANIFI_ROHINGYA)
_c(Makasar, MAKASAR)
_c(Medefaidrin, MEDEFAIDRIN)
_c(OldSogdian, OLD_SOGDIAN)
_c(Sogdian, SOGDIAN)
#endif
#if _c_include_all || _c_include_supported == HB_VERSION_ATLEAST(2, 4, 0)
_c(Elymaic, ELYMAIC)
_c(Nandinagari, NANDINAGARI)
_c(NyiakengPuachueHmong, NYIAKENG_PUACHUE_HMONG)
_c(Wancho, WANCHO)
#endif
#if _c_include_all || _c_include_supported == HB_VERSION_ATLEAST(2, 6, 7)
_c(Chorasmian, CHORASMIAN)
_c(DivesAkuru, DIVES_AKURU)
_c(KhitanSmallScript, KHITAN_SMALL_SCRIPT)
_c(Yezidi, YEZIDI)
#endif
#if _c_include_all || _c_include_supported == HB_VERSION_ATLEAST(3, 0, 0)
_c(CyproMinoan, CYPRO_MINOAN)
_c(OldUyghur, OLD_UYGHUR)
_c(Tangsa, TANGSA)
_c(Toto, TOTO)
_c(Vithkuqi, VITHKUQI)
#endif
#if _c_include_all || _c_include_supported == HB_VERSION_ATLEAST(3, 4, 0)
_c(Math, MATH)
#endif
#if _c_include_all || _c_include_supported == HB_VERSION_ATLEAST(5, 2, 0)
_c(Kawi, KAWI)
_c(NagMundari, NAG_MUNDARI)
#endif
#if _c_include_all || _c_include_supported == HB_VERSION_ATLEAST(10, 0, 0)
_c(Garay, GARAY)
_c(GurungKhema, GURUNG_KHEMA)
_c(KiratRai, KIRAT_RAI)
_c(OlOnal, OL_ONAL)
_c(Sunuwar, SUNUWAR)
_c(Todhri, TODHRI)
_c(TuluTigalari, TULU_TIGALARI)
#endif
#if _c_include_all || _c_include_supported == HB_VERSION_ATLEAST(11, 5, 0)
_c(BeriaErfe, BERIA_ERFE)
_c(Sidetic, SIDETIC)
_c(TaiYo, TAI_YO)
_c(TolongSiki, TOLONG_SIKI)
#endif
/* !IMPORTANT! When adding more scripts, be sure to update the
   HB_VERSION_ATLEAST() macro in doSetScript() to the latest value here so the
   switch doesn't end with a case right before the end, failing compilation
   with older HarfBuzz versions! */
#endif
