/** Kerrighed Checkpoint-Library Interface
 *  @file libkrgcheckpoint.c
 *
 *  @author Matthieu Fertré
 */
#include <kerrighed_tools.h>
#include <libkrgcheckpoint.h>

int cr_disable(void)
{
	return call_kerrighed_services(KSYS_APP_CR_DISABLE, NULL);
}

int cr_enable(void)
{
	return call_kerrighed_services(KSYS_APP_CR_ENABLE, NULL);
}
