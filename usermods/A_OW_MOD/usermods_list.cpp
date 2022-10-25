#include "wled.h"
/*
 * Register your v2 usermods here!
 */
#ifdef A_OW_MOD
#include "../usermods/A_OW_MOD/usermod_A_OW_MOD.h"
#endif

void registerUsermods()
{
#ifdef USERMOD_A_OW_MOD
  usermods.add(new Usermod_A_OW_MOD());
#endif
}