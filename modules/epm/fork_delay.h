/** Fork_Delay
 *  @author Jerome Gallard
 */

#ifdef CONFIG_KRG_FD

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/linkage.h>
#include <linux/dcache.h>
#include <linux/namei.h>

#include <tools/syscalls.h>
#include <ctnr/container.h>
#include <ctnr/container_mgr.h>
#include <kerrighed/types.h>

#include "epm.h"

#include <linux/config.h>

int do_fork_delay_stop_process(struct task_struct *tsk);

extern int send_kerrighed_signal_fd(struct task_struct * tsk, int code);
extern void msleep(unsigned int msecs);

void jeton_destruction(void);
int jeton_gestion(void);
int get_node_idle(void);
int wake_up_pfd(int node_to_fork);
int wake_up_pfd_according_to_resources(void);
void fd_management(void);
int handle_jeton(kerrighed_node_t sender, void *_msg);
int handle_admin(kerrighed_node_t sender, void *_msg);
void case_kerrighed_node_id(void);
void add_1000_to_vload_during_Xms (unsigned int x);
void remove_1000_to_vload_during_Xms (unsigned int x);
struct l_p_delay *check_if_tsk_is_in_pfd_list (struct task_struct *tsk);
void unblock_tsk(struct l_p_delay *l_pfd);
void block_a_pfd(struct l_p_delay *l_pfd);
void add_a_pfd_at_the_end_of_the_list(struct task_struct *tsk);
int check_and_block(struct task_struct *tsk);
     
//#ifdef CONFIG_FD_V2
int check_if_the_local_node_is_idle(void);
int fd_v2_manager_thread (void *dummy);
//#endif //CONFIG_FD_V2

// fork_delay server management
void fd_server_init(void);
void fd_server_finalize(void);

#endif /* CONFIG_KRG_FD */
