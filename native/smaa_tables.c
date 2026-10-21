/* Reference SMAA lookup data, copyright/license in shaders/third_party/smaa. */
#include "../shaders/third_party/smaa/AreaTex.h"
#include "../shaders/third_party/smaa/SearchTex.h"
uint32_t graphene_smaa_texel(uint32_t table, uint32_t index) {
    if (table == 0 && index < AREATEX_WIDTH * AREATEX_HEIGHT)
        return 0xff000000u | areaTexBytes[index*2] | ((uint32_t)areaTexBytes[index*2+1]<<8);
    if (table == 1 && index < SEARCHTEX_WIDTH * SEARCHTEX_HEIGHT)
        return 0xff000000u | searchTexBytes[index];
    return 0;
}
