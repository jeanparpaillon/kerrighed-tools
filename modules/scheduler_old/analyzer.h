/** Mosix probe analyzer.
 *  @file analyzer.h
 *
 *  @author Renaud Lottiaux
 */

#ifndef __ANALYZER_H__
#define __ANALYZER_H__

#include <asm/param.h>

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                             MACRO CONSTANTS                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/

#define MIN_TICKS_BETWEEN_ALARM (2 * HZ)

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

void send_alarm_to_analyzer(void);

void notify_migration_start_to_analyzer(void);
void notify_migration_end_to_analyzer(void);

#endif /* __ANALYZER_H__ */
