/*
 *  Kerrighed/modules/epm/fork_delay.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 */

/** Fork_Delay implementation
 *  
 *  @author Jerome Gallard
 */

#include <linux/sched.h>
#include <kerrighed/kerrighed_signal.h>
#include <asm/siginfo.h>

#include "debug_epm.h"
#define MODULE_NAME "Fork_Delay"


#include <fs/physical_fs.h>
#include <tools/syscalls.h>
#include <ghost/ghost.h>
#include <epm/epm_internal.h>

#include <procfs/dynamic_node_info_linker.h>

#include <scheduler/krg_scheduler.h>

#include <linux/time.h>

#include <scheduler/analyzer.h>
#include "migration.h"

#include <asm/semaphore.h>

#include "fork_delay.h"

/**
 * Declaration of the virtual load
 */
DECLARE_MUTEX (vload_lock);
//int vload = 0;

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                               CONFIG_FD_V1                               *
 *                                                                          *
 *--------------------------------------------------------------------------*/
#ifdef CONFIG_FD_V1
int mutex_fd = 0;
DECLARE_MUTEX (fd_mutex_lock);


/*
 * allow a process in the pfd_list to be executed
 */
void unblock_tsk(struct l_p_delay *l_pfd)
{
  l_pfd->fd_status = PFD_OK;
  nb_process_delayed++; //bien placé ?
}

/*
 * prevent a process in the pfd_list to be executed
 */
void block_a_pfd(struct l_p_delay *l_pfd)
{
  l_pfd->fd_status = PFD_BLOCKED;
  nb_process_delayed--; //bien placé ?
}

/*
 * check if tsk is in the list of pfd
 * return the l_p_delay structure of the pfd if true
 * else return NULL
 */
struct l_p_delay *check_if_tsk_is_in_pfd_list(struct task_struct *tsk)
{
  struct l_p_delay *l_pfd=NULL;
  struct list_head *pos;
  list_for_each(pos,&head_list_fd.lh)
    {
      l_pfd = list_entry(pos, struct l_p_delay, lh);
      if (l_pfd->p == tsk)
	return l_pfd;
    }
  return NULL;
}

/* cr
 * check if tsk is in the list of pfd
 * block it if true and return 1
 * else return 0
 */
int check_and_block(struct task_struct *tsk)
{
  struct l_p_delay *l_pfd = check_if_tsk_is_in_pfd_list(tsk);
  if (l_pfd != NULL)
    {
      block_a_pfd(l_pfd);
      return 1;
    }
  return 0;
}
/* cr
 * check if tsk is in the list of pfd
 * UNblock it if true and return 1
 * else return 0
 */
int check_and_unblock(struct task_struct *tsk)
{
  struct l_p_delay *l_pfd = check_if_tsk_is_in_pfd_list(tsk);
  if (l_pfd != NULL)
    {
      unblock_tsk(l_pfd);
      return 1;
    }
  return 0;
}
/* cr
 * add a pfd at the end of the list
 */
void add_a_pfd_at_the_end_of_the_list(struct task_struct *tsk)
{
  struct l_p_delay *l_pfd;
  l_pfd = (struct l_p_delay *) kmalloc(sizeof(struct l_p_delay),GFP_KERNEL); //GFP_KERNEL ?
  l_pfd->p = tsk;
  l_pfd->fd_status = PFD_OK;
  list_add(&(l_pfd->lh), &(head_list_fd.lh));
  nb_process_delayed++;
}

/*
 * destroy the "jeton"
 * and announce it to all node of the cluster
 */
void jeton_destruction()
{
  int send_msg;

  //  printk("jeton_destruction : envoi de message destruction du jeton\n");
  
  for_each_possible_krgnode(node) 
    {
      if (node != kerrighed_node_id) //pas besoin de s'envoyer a soit meme
	{
	  send_msg = -1;
	  //	  printk("case_kerrighed_node_id : admin : next node : %d, msg : %d\n",next,send_msg);
	  async_remote_service_call ( node, ADMIN,
				      FD_CHAN, &send_msg,
				      sizeof(send_msg) ) ;
	}
    }
  jeton = -1;
  var_chan_admin = -1;
}


/*
 * return the node_id of the first node which the load is low
 * return -1 if no node was found
 */
int get_node_idle()
{
  int load_cpu;
  int tmp;
  krg_dynamic_node_info_t *node_info;
  int msg;


  for_each_possible_krgnode(node)
    {
      node_info = get_dynamic_node_info(node);
      msg = -7; //get the vload of the distant node
      tmp = sync_remote_service_call ( node, VLOAD,
				       FD_CHAN, &msg,
				       sizeof(msg) ) ;
      load_cpu = node_info->mosix_load + tmp;
      if (load_cpu < threshold)
	{
	  //	  	  printk("vload : %d\n", tmp);
	  //	  	  printk("get_node_idle : node : %d, load_cpu+vload = %d\n", i, load_cpu);
	  return node;
	}
    }
  return -1;
}

/*
 * Find a process delayed to wake up if a ressource is available
 * return 1 if a process was woken
 * return 2 if no process was woken whereas there is no jobs in the queue
 * return 0 if there was no job in the queue
 * return -1 if error
 */
int wake_up_pfd_according_to_resources()
{
  kerrighed_node_t node_to_fork;

  //check if we have job in the queue
  if (nb_process_delayed > 0)
    {//yes
      //search if a ressource is avaible
      node_to_fork = get_node_idle();
      if (node_to_fork >= 0)
	{//yes a ressource is available
	  wake_up_pfd(node_to_fork);
	  return 1;
	}//else, i would like to wake up a process, but no ressource was available
      return 2;
    }//else, no job in the queue
  return 0;
}


/*
 * try to consum the "jeton"
 * when it successes, it gives it to his neighbor and return 0
 * and it return -1 in case of failure
 */
int jeton_gestion()
{
  int r;
  kerrighed_node_t next;
  int send_jeton;
  //  do_posix_clock_monotonic_gettime(&uptime);
  //  printk("jeton_gestion : uptime 1 : %d\n",(int) uptime.tv_sec);
  do
    { //there are jobs in the queue, and we are waiting for a process delayed to wake up
      r = wake_up_pfd_according_to_resources();
      msleep(1);
    } while (r == 2);

  if (r == 1)
    {
      //ok, we have consum the "jeton" and wake_up a process,
      //now, pass the "jeton" to my neighbor
      next = krgnode_next_possible_in_ring(kerrighed_node_id);
      send_jeton = kerrighed_node_id;
      async_remote_service_call ( next, JETON,
				  FD_CHAN, &send_jeton,
				  sizeof(send_jeton) ) ;
      jeton = -1;
      //      do_posix_clock_monotonic_gettime(&uptime);
      //      printk("jeton_gestion : uptime 2 : %d\n",(int) uptime.tv_sec);
      return 0;
    }
  else
    {
      printk("jeton_gestion : r = %d : unknown case\n",r);
    }
  return -1;
}


int handle_jeton  (kerrighed_node_t sender,
				  void *_msg)
{
  kerrighed_node_t next;
  int send_jeton;
  int *pjeton = _msg;
  
  if(nb_process_delayed > 0)
    {//I have some works to do
      //i save the "jeton"
      jeton = *pjeton;
      //i try to consum it
      fd_management();
    }
  else
    {//i have no work to do, i not belong to the ring
      //is me the last who consum the "jeton" ?
      if (*pjeton == kerrighed_node_id)
	{//yes, it means that i'm alone on the ring
	  //how i have no more work to do,
	  //i could destroy the "jeton"
	  jeton_destruction();
	}
      else
	{//no, i give the "jeton" to my neighbor
	  next = krgnode_next_possible_in_ring(kerrighed_node_id);
	  send_jeton = *pjeton;
	  async_remote_service_call ( next, JETON,
				      FD_CHAN, &send_jeton,
				      sizeof(send_jeton) ) ;
	  jeton = -1;
	}
    }
  return 0;
}

/*
 * try to wake up a pfd if we
 * some work in the queue and
 * if we have the "jeton"
 * this function is lock by mutex_fd
 * to prevent the execution of several instance of it
 * simultaneously
 */
void fd_management()
{
  kerrighed_node_t next;
  int send_msg;

  down(&fd_mutex_lock);
  //  if (mutex_fd == 0) //the mutex is not taken
  //    {
  //      mutex_fd = 1; //we take the mutex
  //      up(&fd_mutex_lock);

      kerrighed_node_t node_id;
      krg_dynamic_node_info_t *node_info;
      node_id = (kerrighed_node_t) kerrighed_node_id;
      node_info = get_dynamic_node_info(node_id);
      
      if(nb_process_delayed > 0) //i have work to do
	{
	  if(var_chan_admin == -1)
	    {//but there is no "jeton" in the ring
	      //we are ready to start an election for the creation of a new "jeton"
	      
	      next = krgnode_next_possible_in_ring(kerrighed_node_id);
	      send_msg=kerrighed_node_id;
	      async_remote_service_call ( next, ADMIN,
					  FD_CHAN, &send_msg,
					  sizeof(send_msg) ) ;
	    }
	  if(var_chan_admin == -2)
	    {
	      //ok, we know that a "jeton" is on the ring	      
	      //but, have i the "jeton" ?
	      if (jeton >= 0)
		{//yes
		  //i manage the queue of pfd
		  if(jeton_gestion() == -1)
		    {
		      printk("fd_management : ERROR : jeton_gestion\n");
		    }
		}//else, i have not the "jeton", i had to wait my turn => nothing to do
	    }
	}//else, i have nothing to do
      //      down(&fd_mutex_lock);
      //      mutex_fd = 0;
      //    }//else, another instance is already in progress => nothing to do
  up(&fd_mutex_lock);
}


/*
 * this function is called when the current node receive a message (type ADMIN) sending by itself.
 * tipicaly : the message has made a turn on the ring and he should be stopped
 * 2 cases are possible :
 * - if the "jeton" exist, he must be destroyed
 * - if the "jeton" does not exist, he needs to be created
 */
void case_kerrighed_node_id()
{
  int send_msg;
  int other_cases = 1;

  if(var_chan_admin == -1)
    {//there is no "jeton" on the ring,  i will create one !
      var_chan_admin = -2;
      jeton = kerrighed_node_id;
      //and, i inform all the node of the cluster (except me)
      for_each_possible_kgrnode(node)
	{
	  if (node != kerrighed_node_id)
	    {
	      send_msg = -2;
	      async_remote_service_call ( node, ADMIN,
					  FD_CHAN, &send_msg,
					  sizeof(send_msg) ) ;
	    }
	}
      //now, we need to call fd_management to try to consum the "jeton"
      fd_management();
      other_cases = 0;
    }
  if(other_cases == 1 && var_chan_admin == -2)
    {//there is a "jeton" un the ring, we will destroy it !
      jeton_destruction();
      other_cases = 0;
    }
  if(other_cases == 1)
    {
      printk("case_kerrighed_node_id : autre cas : msg unknown\n");
    }
}

int handle_admin  (kerrighed_node_t sender,
				  void *_msg)
{
  int next;
  int send_msg;
  int *msg = _msg;
  int other_cases = 0;

  switch(*msg)
    {   
    case -3 :
      //wake_up the fd_management
      fd_management();
      break;
    case -2 :
      //we save the fact that a "jeton" was created, and now there is a "jeton" on the ring
      var_chan_admin=-2;
      break;
    case -1 :
      //destruction of the "jeton"
      var_chan_admin = -1;
      fd_management(); /* this prevent from a blocking : it's possible to have jobs in the local queue
                        * and we want to execute them now
                        */
      break;
    default :
      other_cases = 1;
    }

  //i receive my own message
  if(other_cases == 1 && *msg == kerrighed_node_id)
    {
      case_kerrighed_node_id();
      other_cases = 0;
    }

  //i receive a message >= 0
  if (other_cases == 1 && *msg >= 0) //ok
    {	 
      //i receive message stronger than var_chan_admin
      //i save it, and i give it to my neighbor
      if (var_chan_admin < *msg )
	{
	  var_chan_admin = *msg;
	  next = krgnode_next_possible_in_ring(kerrighed_node_id);
	  send_msg = *msg;
	  async_remote_service_call ( next, ADMIN,
				      FD_CHAN, &send_msg,
				      sizeof(send_msg) ) ;
	}

      //if  (var_chan_admin > *msg )
      //nothing todo
      
      //if  (var_chan_admin == *msg )
      //nothing todo

      other_cases = 0;

    }
  if (other_cases == 1)
    {
      printk("handle_admin : error : unknown msg\n");
      return -1;
    }
  return 0;
}
#endif //CONFIG_FD_V1

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              CONFIG_KRG_FD                               *
 *                                                                          *
 *--------------------------------------------------------------------------*/


int do_fork_delay_stop_process(struct task_struct *tsk)
{
	struct siginfo info;

	tsk->krg_task->aragorn->aragorn_action = FORK_DELAY_STOP_PROCESS;

	info.si_errno = 0;
	info.si_pid = 0;
	info.si_uid = 0;

	return send_kerrighed_signal(KRG_SIG_FORK_DELAY_STOP, &info, tsk);
}


static void kcb_fork_delay_stop_process(int sig, struct siginfo *info,
					struct pt_regs *regs)
{
	struct aragorn_struct *aragorn_struct = tsk->krg_task->aragorn;
	int send_msg;
	struct l_p_delay *l_pfd;

	DEBUG(DBG_FORK_DELAY, 1, "%s(%d)\n", current->comm, current->pid);

	BUG_ON(aragorn_struct->aragorn_action != FORK_DELAY_STOP_PROCESS);

	//stop PFD until new ressource has available
	//      printk("aragorn_start : debut FORK_DELAY_STOP_PROCESS\n");
	set_current_state(TASK_UNINTERRUPTIBLE);
	cleanup_aragorn_informations(current);

	//add the process at the begining of the list
	l_pfd = (struct l_p_delay *)kmalloc(sizeof(struct l_p_delay), GFP_KERNEL);	//GFP_KERNEL ?
	l_pfd->p = tsk;
	l_pfd->fd_status = PFD_OK;
	list_add(&(l_pfd->lh), &(head_list_fd.lh));
	nb_process_delayed++;
#ifdef CONFIG_FD_V1
	//we need to call fd_management in case of they have no "jeton"
	//on the ring.
	//Perhaps we could do some optimization instead of call fd_management
	//each time.
	send_msg = -3;
	async_remote_service_call(kerrighed_node_id, ADMIN,
					  FD_CHAN, &send_msg, sizeof(send_msg));
#endif				//CONFIG_FD_V1
	send_msg = 12;	//TODO a mettre au propre
	async_remote_service_call(kerrighed_node_id, VLOAD,
				  FD_CHAN, &send_msg, sizeof(send_msg));
#if 0
	up(&aragorn_struct->aragorn_mutex);
#endif
	//      printk("aragorn_start : fin FORK_DELAY_STOP_PROCESS\n");
	//      printk("aragorn_start : FIFO : %d\n", nb_process_delayed);

	schedule();
}


/**
 * wake up a pfd on the node node_to_fork
 * return 0
 */
int wake_up_pfd(int node_to_fork)
{

  struct l_p_delay *l_pfd=NULL;
  struct list_head *pos;
  //  do_posix_clock_monotonic_gettime(&uptime);
  //  printk("jeton_gestion : uptime 1 : %d wpfd\n",(int) uptime.tv_sec);
  //now, we will get the first process on the list
  //who is NOT blocked
  list_for_each(pos,&head_list_fd.lh)
    {
      l_pfd = list_entry(pos, struct l_p_delay, lh);
      if (l_pfd->fd_status == PFD_OK)
	break; //get just de first one NOT blocked
    }

/*
 * WARNING "TOREMOVE when Kerrighed will be able to migrate a process more than just one time"
 */
  if (node_to_fork != kerrighed_node_id &&  l_pfd->p->krg_task->aragorn->nb_migration > 0)
    {
      printk("fork_delay.c : wake_up_pfd : this process has already been migrated, we can not do it a second time\n");
      return -1;
    }
//end warning  

  int msg;
  int load_cpu;
  kerrighed_node_t node_id;
  krg_dynamic_node_info_t *node_info;
  node_id = (kerrighed_node_t)node_to_fork;
  node_info = get_dynamic_node_info(node_id);
  load_cpu = node_info->mosix_load;
  //	  printk("find_a_process_delayed_to_wake_up : id : %d, load_cpu : %d\n",node_to_fork, load_cpu);
  
  
  switch (l_pfd->p->fd_flag)
    {
    case PFD_NOT_ALREADY_WU: //pfd not already wake_up
      //	      printk("find_a_process_delayed_to_wake_up : PFD_NOT_ALREADY_WU\n");
      wake_up_new_task(l_pfd->p, l_pfd->clone_flags);
      do_posix_clock_monotonic_gettime(&l_pfd->p->start_time);
      l_pfd->p->fd_flag=PFD_ALREADY_WU;
      break;
    case PFD_ALREADY_WU: //pfd already wake_up and stopped
      //	      send_kerrighed_signal_fd(l_pfd->p,1);
      //	      printk("find_a_process_delayed_to_wake_up : PFD_ALREADY_WU\n");
      wake_up_process(l_pfd->p);
      break;
    default:
      printk("wake_up_pfd : ERROR unknown message : %d\n",l_pfd->p->fd_flag);
      return -1;
    }
  
  if (node_to_fork != kerrighed_node_id)
    {
      //	      printk("find_a_process_delayed_to_wake_up : migration du process sur le noeud : %d\n", node_to_fork);


      notify_migration_start_to_analyzer() ;
      int val = do_migrate_process(l_pfd->p, node_to_fork);
      //	      printk("val : %d\n",val);
      notify_migration_end_to_analyzer() ;
      
      if (val == 0)
	{//migration ok
	  //monte l'offset de 1000
	  msg = -5;
#ifdef CONFIG_FD_V1
	  sync_remote_service_call ( node_to_fork, VLOAD,
				      FD_CHAN, &msg,
				      sizeof(msg) ) ;
#endif
#ifdef CONFIG_FD_V2
	  sync_remote_service_call ( node_to_fork, VLOAD,
				      FD_CHAN, &msg,
				      sizeof(msg) ) ;
#endif
#ifdef CONFIG_FD_V1
	  sync_remote_service_call ( kerrighed_node_id, VLOAD, //le process est forké sur le noeud local
				      FD_CHAN, &msg, //il doit migrer, il va faire un exit donc un vload - 1000
				      sizeof(msg) ) ; //on anticipe avec un vload+1000
#endif
	}
      else
	{
	  printk("wake_up_pfd : can not migrate the FD process %s (%d)\n",l_pfd->p->comm,l_pfd->p->pid);
	  printk("wake_up_pfd : check capacity ?\n");
	}
      //	      printk("find_a_process_delayed_to_wake_up : migration du process OK !\n");
    }
  else
    {//node_to_fork == kerrighed_node_id
      msg = -5;
#ifdef CONFIG_FD_V1
      sync_remote_service_call ( kerrighed_node_id, VLOAD,
				  FD_CHAN, &msg,
				  sizeof(msg) ) ;
#endif
#ifdef CONFIG_FD_V2
      sync_remote_service_call ( kerrighed_node_id, VLOAD,
				  FD_CHAN, &msg,
				  sizeof(msg) ) ;
#endif
    }
  struct list_head *q;
  list_for_each_safe(pos, q, &head_list_fd.lh)
    {
      l_pfd = list_entry(pos, struct l_p_delay, lh);
      list_del(pos);
      kfree(l_pfd);
      break; //del just the first one !
    }
  
  nb_process_delayed--;
  //  do_posix_clock_monotonic_gettime(&uptime);
  //  printk("jeton_gestion : uptime 1 : %d wpfd\n",(int) uptime.tv_sec);
  return 0;
}


void send_a_message_to_the_local_node(int msg, int type)
{
  sync_remote_service_call ( kerrighed_node_id, type,
			      FD_CHAN, &msg,
			      sizeof(msg) ) ;
}

void send_a_message_to_the_next_node(int msg, int type)
{
	sync_remote_service_call ( krgnode_next_online(kerrighed_node_id),
				   type,
				   FD_CHAN, &msg,
				   sizeof(msg) ) ;
}

/**
 * return the load+vload of the node node_id
 */
int get_load_of_node(int node_id)
{
  int load_cpu;
  int tmp;
  krg_dynamic_node_info_t *node_info;
  int msg;


  node_info = get_dynamic_node_info(node_id);
  msg = -7; //get the vload of the distant node
  tmp = sync_remote_service_call ( kerrighed_node_id, VLOAD,
				   FD_CHAN, &msg,
				   sizeof(msg) ) ;
  load_cpu = node_info->mosix_load + tmp;
  return load_cpu;
}

void add_1000_to_vload_during_Xms (unsigned int x)
{
  //  printk("vload+1000\n");
  vload = vload + 1000;
  msleep(x);
  vload = vload - 1000;
}

void remove_1000_to_vload_during_Xms (unsigned int x)
{
  //  printk("vload-1000\n");
  vload = vload - 1000;
  msleep(x);
  vload = vload + 1000;
}



int handle_vload  (kerrighed_node_t sender,
				  void *_msg)
{
  int *valeur = _msg;
  int msg;
  int other_case = 0;
  int tmp;
  //  int wait_time = 3000;

 switch(*valeur)
    {   
    case -7:
      //to get the vload
      tmp=vload;
      //      if (tmp < 0)
	//tmp=0;
	//printk("PROBLEME VLOAD NEGATIF\n");
      return tmp;
    case -5: 
      //create a virtual load to prevent the wake up of multiple processus delayed
      //add_1000_to_vload_during_Xms(2000);
      down(&vload_lock);
      vload = vload + 1000;
      //      printk("+ a\n");
      up(&vload_lock);
      msg = -4; //get the vload of the distant node
      async_remote_service_call ( kerrighed_node_id, VLOAD,
				  FD_CHAN, &msg,
				  sizeof(msg) ) ;
      break;
    case -4:
      msleep(wait_time);
      down(&vload_lock);
      vload = vload - 1000;
      //      printk("- b\n");
/*       if (vload < 0) */
/* 	{ */
/* 	  vload = 0; */
/*       printk("0 c\n"); */
/* 	} */
      up(&vload_lock);
      break;
    case -2:
      msleep(wait_time);
      down(&vload_lock);
      vload = vload + 1000;
      //      printk("+ d\n");
      up(&vload_lock);
      break;
    default:
      other_case = 1;
    }
 if (other_case == 1)
   {
     //     remove_1000_to_vload_during_Xms(2000); //x
     //     printk("autre cas\n");
      down(&vload_lock);
      vload = vload - 1000;
      //      printk("- e\n");
/*       if(vload < 0) */
/* 	{ */
/* 	  vload = 0; */
/*       printk("0 f\n"); */
      //up(&vload_lock);
/* 	} */
/*       else */
/* 	{ */
	  up(&vload_lock);
	  msg = -2;
	  async_remote_service_call ( kerrighed_node_id, VLOAD,
				      FD_CHAN, &msg,
				      sizeof(msg) ) ;
/* 	} */
#if 0
     if (*valeur >= 0)
       {
	 do_posix_clock_monotonic_gettime(&uptime);
	 int sec = uptime.tv_sec;
	 int nano = uptime.tv_nsec; //long ? verifier la portabilite en 64 bits
	 int mili = nano / 1000000;
	 int int_uptime = sec*1000 + mili;
	 unsigned int x = int_uptime - *valeur;
	 if (x <= 2000) //ms
	   {//short process
	     remove_1000_to_vload_during_Xms(3000); //x
	   }//else, participation to the vload already removed, nothing to do
       }
     else
       {
	 printk("handle_offset : ERROR : unknown message\n");
       }
#endif //0
   }
 return 0;
}


/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              CONFIG_FD_V2                                *
 *                                                                          *
 *--------------------------------------------------------------------------*/
#ifdef CONFIG_FD_V2
int fd_v2_manager_thread_var; //stop and start the fd_v2_manager_thread
enum{ //state of the node
  FD_IDLE,
    FD_NOT_IDLE,
    };
int node_status[255]; //TODO mettre un truc genre kerrighed_nb_nodes

/**
 * check if the local node is idle
 * return 1 if it is true
 * return 0 else.
 */
int check_if_the_local_node_is_idle()
{
  //  int threshold = 1300; //TODO a voir
  int load_cpu;

  load_cpu = get_load_of_node(kerrighed_node_id);
  if (load_cpu < threshold)
    {
      return 1;
    }

  return 0;
}

int fd_v2_manager_thread (void *dummy)
{
  kernel_thread_setup("FD V2 Mgr");

  for_each_possible_krgnode(node)
    {
      node_status[node] = FD_IDLE;
    }

  while (fd_v2_manager_thread_var)
    {
      msleep(1); //TODO a voir
      if(check_if_the_local_node_is_idle() == 1)
	{//yes he is idle,
	  //	  printk("1 fd_v2_manager_thread : node idle\n");
	  if (node_status[kerrighed_node_id] == FD_IDLE)
	    {
	      //	      printk("2 fd_v2_manager_thread : status IDLE\n");
	      if(nb_process_delayed > 0)
		{//works are waiting...
		  //		  printk("3 fd_v2_manager_thread : job in queue\n");
		  wake_up_pfd(kerrighed_node_id);
		}
	      //	      else
	      //{
	      //  printk("4 fd_v2_manager_thread_var : no job in queue : nothing todo\n");
	      //}
	    }
	  //	  if (node_status[kerrighed_node_id] == NOT_READY)
	  else
	    {
	      //	      printk("5 fd_v2_manager_thread : status NOT_IDLE\n");
	      if (nb_process_delayed > 0)
		{
		  //		  printk("6 fd_v2_manager_thread : job in queue\n");
		  wake_up_pfd(kerrighed_node_id);
		}
	      else
		{//we need to update the status of the node with all the cluster
		  //		  printk("7 fd_v2_manager_thread : no job in queue : update status\n");
		  send_a_message_to_the_local_node(kerrighed_node_id, READY);
		}
	    }
	}
      else
	{
	  //	  printk("8 fd_v2_manager_thread : node NOT_IDLE\n");
	  if (node_status[kerrighed_node_id] != FD_NOT_IDLE)
	    //if (node_status[kerrighed_node_id] == FD_NOT_IDLE)
	    //	    {
	    //printk("9 fd_v2_manager_thread_var : status NOT_IDLE : nothing todo\n");
	    //}
	  //	  if (node_status[kerrighed_node_id] == READY)
	    //	  else
	    {
	      //	      printk("10 fd_v2_manager_thread : status : IDLE : update status\n");
	      send_a_message_to_the_local_node(kerrighed_node_id, NOT_READY);
	    }
	  if (nb_process_delayed > 0)
	    {
	      //	      printk("fd_v2_manager_thread : JOB IN QUEUE !!!!\n");
		    for_each_possible_krgnode(node)
		{
		  if ( node_status[node] == FD_IDLE )
		    {
		      //		      printk("fd_v2_manager_thread : node %d glande ke dal\n",i);
		      wake_up_pfd(node);
		      //		      printk("fd_v2_manager_thread : break\n");
		      break;
		    }
		}
	    }
	}
    }
  
  return 0;
}

int handle_ready  (kerrighed_node_t sender,
				  void *_msg)
{
  int *msg = _msg;
  //  printk("handle_ready %d\n",*msg);
  if (sender != kerrighed_node_id && *msg == kerrighed_node_id)
    {//the msg have made a loop
      //      printk("handle_ready : loop, nothing todo\n");
      return -1;
    }
  //update status
  if (nb_process_delayed > 0)
    {
      //      printk("handle_ready : wake_up a pfd\n");
      wake_up_pfd(*msg);
      return 1;
    }
  //  printk("handle_ready : pas de process a reveiller\n");
  node_status[*msg] = FD_IDLE;
  send_a_message_to_the_next_node(*msg, READY);
  return 0;
}

int handle_not_ready  (kerrighed_node_t sender,
				  void *_msg)
{
  int *msg = _msg;
  //  printk("handle_not_ready : %d\n",*msg);
  if (sender != kerrighed_node_id && *msg == kerrighed_node_id)
    {//the msg have made a loop
      //      printk("handle_not_ready : loop, nothing todo\n");
      return -1;
    }
  //update status
  node_status[*msg] = FD_NOT_IDLE;
  //  printk("handle_not_ready : node_status maj : %d\n",node_status[*msg]);
  send_a_message_to_the_next_node(*msg, NOT_READY);
  //  printk("handle_not_ready : message envoye au suivant\n");
  return 0;
}
#endif //CONFIG_FD_V2

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                      FORK_DELAY SERVER MANAGEMENT                        *
 *                                                                          *
 *--------------------------------------------------------------------------*/

void register_fork_delay_hooks(void)
{
	hook_register(&kh_krg_handler[KRG_SIG_FORK_DELAY_STOP],
		      kcb_fork_delay_stop_process);
}


int epm_fork_delay_start(void)
{
  DEBUG(DBG_FORK_DELAY, 1,"Fork_Delay Server init\n");
  printk("fd_server_init\n");

#ifdef CONFIG_FD_V1
  register_node_service(FD_CHAN, JETON,
			 handle_jeton,
			 sizeof (int));
  register_node_service(FD_CHAN, ADMIN,
			 handle_admin,
			 sizeof (int));
#endif //CONFIG_FD_V1
  register_node_service(FD_CHAN, VLOAD,
			 handle_vload,
			 sizeof (int));
#ifdef CONFIG_FD_V2
  register_node_service(FD_CHAN, READY,
			 handle_ready,
			 sizeof (int));
  register_node_service(FD_CHAN, NOT_READY,
			 handle_not_ready,
			 sizeof (int));
#endif //CONFIG_FD_V2
  rename_service_manager(FD_CHAN, "Fork_Delay Server") ;
  multithread_service_manager(FD_CHAN);

#ifdef CONFIG_FD_V2
  fd_v2_manager_thread_var = 1;
  kernel_thread (fd_v2_manager_thread, NULL, CLONE_VM);
#endif //CONFIG_FD_V2

  DEBUG(DBG_FORK_DELAY, 1,"Done\n");

  return 0;
}


void epm_fork_delay_exit(void)
{
#ifdef CONFIG_FD_V1
  unregister_node_service(FD_CHAN, JETON);
  unregister_node_service(FD_CHAN, ADMIN);
#endif //CONFIG_FD_V1
  unregister_node_service(FD_CHAN, VLOAD);
#ifdef CONFIG_FD_V2
  fd_v2_manager_thread_var = 0;
#endif //CONFIG_FD_V2
}
