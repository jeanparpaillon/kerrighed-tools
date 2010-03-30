/** Kerrighed Callback-Library Interface
 *  @file libkrgcb.h
 *
 *  @author Eugen Feller, John Mehnert-Spahn
 */

#ifndef LIBKRG_CB_H
#define LIBKRG_CB_H

#ifdef __cplusplus
extern "C" {
#endif

/* callback function type */
typedef int (*cr_cb_callback_t)(void *);

/* library init/exit function */
int cr_callback_init(void);
void cr_callback_exit(void);

/* library execute cb functions */
int cr_execute_chkpt_callbacks(long pid, short from_appid);
int cr_execute_restart_callbacks(long pid);
int cr_execute_continue_callbacks(long pid, short from_appid);

/* register checkpoint, restart and continue callbacks out of signal
 * handler context */
int cr_register_chkpt_callback(cr_cb_callback_t func, void *arg);
int cr_register_restart_callback(cr_cb_callback_t func, void *arg);
int cr_register_continue_callback(cr_cb_callback_t func, void *arg);

/* register checkpoint, restart and continue callbacks out of thread
 * context */
int cr_register_chkpt_thread_callback(cr_cb_callback_t func, void *arg);
int cr_register_restart_thread_callback(cr_cb_callback_t func, void *arg);
int cr_register_continue_thread_callback(cr_cb_callback_t func, void *arg);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LIBKRG_CB_H */
