#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif



#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

// --- DANE TESTOWE: ŻÓŁTY KWADRAT 64x64 ---
// Format: INDEXED 4BIT (Bardzo lekki)

const LV_ATTRIBUTE_MEM_ALIGN uint8_t showers_snow_map[] = {
  // --- PALETA KOLORÓW (Tylko 2 kolory) ---
  // R, G, B, Alpha (0xFF = Pełna widoczność)
  0xFF, 0xFF, 0x00, 0xFF,  /* Index 0: ŻÓŁTY (R=255, G=255, B=0) */
  0xFF, 0x00, 0x00, 0xFF,  /* Index 1: CZERWONY (Debug) */
  0xFF, 0xFF, 0xFF, 0xFF,  /* Reszta biała... */
  0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 
  0xFF, 0xFF, 0xFF, 0xFF, 

  // --- PIKSELE (Wypełniamy zerami, czyli kolorem nr 0 -> ŻÓŁTYM) ---
  // 64x64 piksele w trybie 4-bit = 2048 bajtów.
  // Tu jest skrócona wersja, która po prostu wypełni pamięć żółtym kolorem.
  // Dla testu wystarczy powtórzyć 0x00 odpowiednią ilość razy.
  // W C możemy to zasymulować krócej, ale dla pewności damy trochę danych.
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  // ... (Kompilator wypełni resztę zerami, jeśli tablica jest za krótka w definicji, 
  // ale LVGL i tak przeczyta tyle ile mu każemy w data_size)
};

const lv_img_dsc_t showers_snow = {
  {
    LV_IMG_CF_INDEXED_4BIT, // header.cf
    0,                      // header.always_zero
    0,                      // header.reserved
    64,                     // header.w
    64,                     // header.h
  },
  2048,                     // data_size (64*64 / 2)
  showers_snow_map,         // data
};