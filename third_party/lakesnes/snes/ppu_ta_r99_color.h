/* R99: retain exact five-bit byte lanes when the CGRAM palette is built.
 * This avoids recovering them from expanded RGB output for every subtraction.
 * The existing palette invalidation covers CGRAM writes/reset/brightness.
 * Called only for the R50 full-brightness Mode-2 subtract/no-half output. */
#if defined(TA_R99_PALETTE_LANES) && TA_R99_PALETTE_LANES
static inline uint32_t ppu_taR99Subtract(uint32_t a,uint32_t b) {
  const uint32_t d=(a|0x00202020u)-b;
  const uint32_t valid=((d>>5)&0x00010101u)*31u;
  const uint32_t five=d&valid;
  return (five<<3)|((five>>2)&0x00070707u);
}
#endif
