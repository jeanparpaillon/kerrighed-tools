/** Kerrighed FAF Hooks.
 *  @file file_hooks.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */
#include "debug_faf.h"

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/socket.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <kerrighed/signal.h>
#include <kerrighed/faf.h>
#include <asm/uaccess.h>

#include <hotplug/hotplug.h>
#include <rpc/rpcid.h>
#include <rpc/rpc.h>
#include <fs/file.h>
#include <fs/file_struct_io_linker.h>
#include <ctnr/kddm.h>

#include "faf.h"
#include "faf_server.h"
#include "faf_tools.h"
#include "faf_file_mgr.h"
#include "ruaccess.h"



static DEFINE_MUTEX(faf_poll_mutex);



/** Unpack the result value from a distant FAF operation with respect to
 *  distant signals.
 *  @author Renaud Lottiaux
 * 
 *  @param desc     The RPC descriptor to get the value from.
 */
static ssize_t unpack_res (struct rpc_desc* desc)
{
	int err, flags = RPC_FLAGS_INTR;
	ssize_t r;

retry:
	err = rpc_unpack(desc, flags, &r, sizeof(r));
	switch(err) {
	  case RPC_EOK:
		  break;
	  case RPC_EINTR:
		  BUG_ON(flags != RPC_FLAGS_INTR);
		  rpc_signal(desc, SIGINT);
		  /* We do not need to explicitly receive SIGACK, since the
 		   * server will return the result anyway. */
 		  flags = 0;
		  goto retry;
	  default:
		  BUG();
	}

	return r;
}



/** Kerrighed kernel hook for FAF lseek function.
 *  @author Renaud Lottiaux
 * 
 *  @param file    File to seek in.
 *  @param offset  Offset to seek at.
 *  @param origin  Origin of the seek.
 */
off_t kcb_faf_lseek (struct file * file,
		     off_t offset,
		     unsigned int origin)
{
	faf_client_data_t *data = file->private_data;
	faf_seek_msg_t msg;
	off_t r;
	struct rpc_desc* desc;
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);
	
	msg.server_fd = data->server_fd;
	msg.offset = offset;
	msg.origin = origin;
	
	desc = rpc_begin(RPC_FAF_LSEEK, data->server_id);
	
	rpc_pack_type(desc, msg);
	
	rpc_unpack_type(desc, r);
	
	rpc_end(desc, 0);
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
}



/** Kerrighed kernel hook for FAF llseek function.
 *  @author Renaud Lottiaux
 * 
 *  @param file          File to seek in.
 *  @param offset_high   High part of the offset to seek at.
 *  @param offset_low    Low part of the offset to seek at.
 *  @param result        ...
 *  @param origin        Origin of the seek.
 */
long kcb_faf_llseek (struct file *file,
		     unsigned long offset_high,
		     unsigned long offset_low,
		     loff_t * result,
		     unsigned int origin)
{
	faf_client_data_t *data = file->private_data;
	faf_llseek_msg_t msg;
	long r;
	struct rpc_desc* desc;
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);
	
	msg.server_fd = data->server_fd;
	msg.offset_high = offset_high;
	msg.offset_low = offset_low;
	msg.origin = origin;
	
	desc = rpc_begin(RPC_FAF_LLSEEK, data->server_id);
	
	rpc_pack_type(desc, msg);
	
	rpc_unpack_type(desc, r);
	rpc_unpack(desc, 0, result, sizeof(*result));
	
	rpc_end(desc, 0);
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
}



/** Kerrighed kernel hook for FAF read function.
 *  @author Renaud Lottiaux
 * 
 *  @param file          File to read from.
 *  @param buf           Buffer to store data in.
 *  @param count         Number of bytes to read.
 */
ssize_t kcb_faf_read (struct file * file,
		      char *buf,
		      size_t count)
{
	faf_client_data_t *data = file->private_data;
	faf_rw_msg_t msg;
	ssize_t r;
	char *kbuff;
	int err;
	struct rpc_desc* desc;
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);
	
	kbuff = kmalloc (count, GFP_KERNEL);
	if (!kbuff)
		return -ENOMEM;
	
	msg.server_fd = data->server_fd;
	msg.count = count;
	
	desc = rpc_begin(RPC_FAF_READ, data->server_id);
	
	rpc_pack_type(desc, msg);

	r = unpack_res(desc);

	if(r>0) {
		rpc_unpack(desc, 0, kbuff, r);
		
		err = copy_to_user (buf, kbuff, r);
		BUG_ON(err);
		if (err)
			r = -EFAULT;
	};

	rpc_end(desc, 0);
	
	kfree (kbuff);
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
	
}



/** Kerrighed kernel hook for FAF write function.
 *  @author Renaud Lottiaux
 * 
 *  @param file          File to write to.
 *  @param buf           Buffer of data to write.
 *  @param count         Number of bytes to write.
 */
ssize_t kcb_faf_write (struct file * file, 
		       const char *buf,
		       size_t count)
{
	faf_client_data_t *data = file->private_data;
	faf_rw_msg_t msg;
	ssize_t r = 0;
	char *kbuff;
	int err;
	struct rpc_desc* desc;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	kbuff = kmalloc (count, GFP_KERNEL);
	if (!kbuff)
		return -ENOMEM;
	
	err = copy_from_user (kbuff, buf, count);
	BUG_ON(err); /* TODO */
	
	msg.server_fd = data->server_fd;
	msg.count = count;
	
	desc = rpc_begin(RPC_FAF_WRITE, data->server_id);
	
	rpc_pack_type(desc, msg);
	
	rpc_pack(desc, 0, kbuff, count);
	
	kfree (kbuff);

	r = unpack_res(desc);
	
	rpc_end(desc, 0);
	
	if(r == -EPIPE) {
		send_sig(SIGPIPE, current, 0);
	};
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
}



/** Kerrighed kernel hook for FAF ioctl function.
 *  @author Renaud Lottiaux
 * 
 *  @param file          File to do an ioctl to.
 *  @param cmd           IOCTL command.
 *  @param arg           IOCTL argument.
 */
long kcb_faf_ioctl (struct file *file,
		    unsigned int cmd,
		    unsigned long arg)
{
	faf_client_data_t *data = file->private_data;
	faf_ctl_msg_t msg;
	long r;
	struct rpc_desc *desc;
	int err;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	msg.server_fd = data->server_fd;
	msg.cmd = cmd;
	msg.arg = arg;

	err = -ENOMEM;
	desc = rpc_begin(RPC_FAF_IOCTL, data->server_id);
	if (!desc)
		goto out_err;

	err = rpc_pack_type(desc, msg);
	if (err)
		goto out_cancel;
	err = handle_ruaccess(desc);
	if (err)
		goto out_cancel;
	err = rpc_unpack_type(desc, r);
	if (err)
		goto out_cancel;
	rpc_end(desc, 0);

out:
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);

	return r;

out_cancel:
	rpc_cancel(desc);
	rpc_end(desc, 0);
	if (err > 0)
		err = -ENOMEM;
out_err:
	r = err;
	goto out;
}



/** Kerrighed kernel hook for FAF fcntl function.
 *  @author Renaud Lottiaux
 * 
 *  @param file          File to do an fcntl to.
 *  @param cmd           FCNTL command.
 *  @param arg           FCNTL argument.
 */
long kcb_faf_fcntl (struct file *file,
		    unsigned int cmd,
		    unsigned long arg)
{
	faf_client_data_t *data = file->private_data;
	faf_ctl_msg_t msg;
	struct rpc_desc *desc;
	int err;
	long r;
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);
	
	msg.server_fd = data->server_fd;
	msg.cmd = cmd;
	r = -EFAULT;
	if ((cmd == F_SETLK || cmd == F_SETLKW || cmd == F_GETLK)
	    && copy_from_user(&msg.flock,
			      (struct flock __user *) arg, sizeof(msg.flock)))
			goto out;
	else
		msg.arg = arg;

	r = -ENOLCK;
	desc = rpc_begin(RPC_FAF_FCNTL, data->server_id);
	if (unlikely(!desc))
		goto out;

	err = rpc_pack_type(desc, msg);
	if (unlikely(err))
		goto cancel;

	err = rpc_unpack_type(desc, r);
	if (unlikely(err))
		goto cancel;

	if (!r && cmd == F_GETLK) {
		err = rpc_unpack_type(desc, msg.flock);
		if (unlikely(err))
			goto cancel;
		r = -EFAULT;
		if (!copy_to_user((struct flock __user *) arg,
				  &msg.flock, sizeof(msg.flock)))
			r = 0;
	}

out_end:
	rpc_end(desc, 0);

out:
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	return r;

cancel:
	rpc_cancel(desc);
	goto out_end;
}



#if BITS_PER_LONG == 32
/** Kerrighed kernel hook for FAF fcntl64 function.
 *  @author Renaud Lottiaux
 * 
 *  @param file          File to do an fcntl to.
 *  @param cmd           FCNTL command.
 *  @param arg           FCNTL argument.
 */
long kcb_faf_fcntl64 (struct file *file,
		      unsigned int cmd,
		      unsigned long arg)
{
	faf_client_data_t *data = file->private_data;
	faf_ctl_msg_t msg;
	long r;
	struct rpc_desc* desc;
	int err;
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);
	
	msg.server_fd = data->server_fd;
	msg.cmd = cmd;
	r = -EFAULT;
	if ((cmd == F_GETLK64 || cmd == F_SETLK64 || cmd == F_SETLKW64)
	    && copy_from_user(&msg.flock64,
			      (struct flock64 __user *) arg, sizeof(msg.flock64)))
			goto out;
	else
		msg.arg = arg;
	
	r = -ENOLCK;
	desc = rpc_begin(RPC_FAF_FCNTL64,
			 data->server_id);
	if (unlikely(!desc))
		goto out;
	
	err = rpc_pack_type(desc, msg);
	if (unlikely(err))
		goto cancel;

	err = rpc_unpack_type(desc, r);
	if (unlikely(err))
		goto cancel;

	if (!r && cmd == F_GETLK64) {
		err = rpc_unpack_type(desc, msg.flock64);
		if (unlikely(err))
			goto cancel;
		r = -EFAULT;
		if (!copy_to_user((struct flock64 __user *) arg,
				  &msg.flock64, sizeof(msg.flock64)))
			r = 0;
	}

out_end:
	rpc_end(desc, 0);
	
out:
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;

cancel:
	rpc_cancel(desc);
	goto out_end;
}
#endif



/** Kerrighed kernel hook for FAF fstat function.
 *  @author Renaud Lottiaux
 * 
 *  @param file          File to do an fcntl to.
 *  @param statbuf       Kernel buffer to store file stats.
 */
long kcb_faf_fstat (struct file *file,
		    struct kstat *statbuf)
{
	struct kstat buffer;
	faf_client_data_t *data = file->private_data;
	faf_stat_msg_t msg;
	long r;
	struct rpc_desc* desc;
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);
	
	msg.server_fd = data->server_fd;
	
	desc = rpc_begin(RPC_FAF_FSTAT, data->server_id);
	
	rpc_pack_type(desc, msg);
	
	rpc_unpack_type(desc, r);
	rpc_unpack_type(desc, buffer);
	
	rpc_end(desc, 0);
	
	*statbuf = buffer;
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
}



/** Kerrighed kernel hook for FAF fsync function.
 *  @author Renaud Lottiaux
 * 
 *  @param file          File to do a fsync to.
 */
long kcb_faf_fsync (struct file *file)
{
	faf_client_data_t *data = file->private_data;
	faf_rw_msg_t msg;
	long r;
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);
	
	msg.server_fd = data->server_fd;
	
	r = rpc_sync(RPC_FAF_FSYNC, data->server_id, &msg, sizeof(msg));
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
}



/** Kerrighed kernel hook for FAF flock function.
 *  @author Renaud Lottiaux
 * 
 *  @param file          File to do a flock to.
 */
long kcb_faf_flock (struct file *file,
		    unsigned int cmd)
{
	faf_client_data_t *data = file->private_data;
	faf_ctl_msg_t msg;
	long r;
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	msg.server_fd = data->server_fd;
	msg.cmd = cmd;

	r = rpc_sync(RPC_FAF_FLOCK, data->server_id, &msg, sizeof(msg));
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);

	return r;
}



/** Kerrighed FAF d_path function.
 *  @author Renaud Lottiaux
 * 
 *  @param file     The file to get the path.
 *  @param buff     Buffer to store the path in.
 */
char *faf_d_path (struct file *file,
                  char *buff,
                  int size)
{
	faf_client_data_t *data = file->private_data;
	faf_rw_msg_t msg;
	struct rpc_desc* desc;
	
	DEBUG ("server", 2, -1, faf_srv_id(file), faf_srv_fd(file),
	       file->f_objid, NULL, file, FAF_LOG_ENTER, 0);

	if (file->f_flags & O_FAF_SRV)
		return d_path(file->f_path.dentry, file->f_path.mnt, buff,
			      size);
	
	msg.server_fd = data->server_fd;
	msg.count = size;
	
	desc = rpc_begin(RPC_FAF_D_PATH, data->server_id);
	rpc_pack_type(desc, msg);
	rpc_unpack(desc, 0, buff, size);
	rpc_end(desc, 0);
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, 0);
	
	return buff;
}



long kcb_faf_bind (struct file * file,
		   struct sockaddr __user *umyaddr,
		   int addrlen)
{
	faf_client_data_t *data = file->private_data;
	faf_bind_msg_t msg;
	long r;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	msg.server_fd = data->server_fd;

	if((r=move_addr_to_kernel(umyaddr,
				  addrlen,
				  &msg.sa))>=0) {
		msg.addrlen = addrlen;
		r = rpc_sync(RPC_FAF_BIND, data->server_id, &msg, sizeof(msg));
	};

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
};



long kcb_faf_connect (struct file * file,
		      struct sockaddr __user *uservaddr,
		      int addrlen)
{
	faf_client_data_t *data = file->private_data;
	faf_bind_msg_t msg;
	long r;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	msg.server_fd = data->server_fd;

	if((r=move_addr_to_kernel(uservaddr,
				  addrlen,
				  &msg.sa))>=0) {
		msg.addrlen = addrlen;
		r = rpc_sync(RPC_FAF_CONNECT, data->server_id, &msg, sizeof(msg));
	};

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
};



long kcb_faf_listen (struct file * file,
		     int backlog)
{
	faf_client_data_t *data = file->private_data;
	faf_listen_msg_t msg;
	int r;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	msg.server_fd = data->server_fd;
	
	msg.backlog = backlog;

	r = rpc_sync(RPC_FAF_LISTEN, data->server_id, &msg, sizeof(msg));

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
};



long kcb_faf_accept (struct file * file,
		     struct sockaddr __user *upeer_sockaddr,
		     int __user *upeer_addrlen)
{
	struct dvfs_file_struct *dvfs_file;
	faf_client_data_t *data = file->private_data;
	faf_bind_msg_t msg;
	int r;
	char sa[MAX_SOCK_ADDR];
	int sa_len;
	void *fdesc;
	int desc_len;
	struct file *newfile;
	int fd;
	struct rpc_desc* desc;
	unsigned long objid;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	BUG_ON (data->server_id == kerrighed_node_id);

	fd = get_unused_fd();
	if (fd < 0) {
		r = fd;
		goto out;
	}

	msg.server_fd = data->server_fd;
	
	if (upeer_sockaddr) {
		if (get_user(msg.addrlen, upeer_addrlen)) {
			r = -EFAULT;
			goto out_put_fd;
		}
	} else {
		msg.addrlen = 0;
	}

	desc = rpc_begin(RPC_FAF_ACCEPT, data->server_id);

	rpc_pack_type(desc, msg);

	rpc_unpack_type(desc, r);

	if(r<0){
		rpc_end(desc, 0);
		goto out_put_fd;
	};
	
	rpc_unpack_type(desc, desc_len);

	fdesc = kmalloc(desc_len, GFP_KERNEL);
	if (!fdesc) {
		r = -ENOMEM;
		goto err_cancel;
	}

	rpc_unpack(desc, 0, fdesc, desc_len);
	rpc_unpack_type(desc, sa_len);
	rpc_unpack(desc, 0, &sa, sa_len);
	rpc_unpack_type(desc, objid);
	
	newfile = create_faf_file_from_krg_desc(current, fdesc);
	kfree(fdesc);
	if (!newfile) {
		r = -ENOMEM;
		goto err_cancel;
	}

	/*
	 * We have enough to clean up the new file ourself if needed. Tell it
	 * to the server.
	 */
	rpc_pack_type(desc, fd);
	rpc_end(desc, 0);

	dvfs_file = grab_dvfs_file_struct (objid);
	
	BUG_ON (dvfs_file->file != NULL);
	newfile->f_objid = objid;
	dvfs_file->file = newfile;
	
	DEBUG ("server", 2, -1, data->server_id, data->server_fd, objid, 
	       NULL, newfile, FAF_LOG_ACCEPT_DONE, dvfs_file);

	put_dvfs_file_struct (objid);

	if (upeer_sockaddr) {
		r = move_addr_to_user(&sa, sa_len,
				      upeer_sockaddr, upeer_addrlen);
		if (r)
			goto err_close_faf_file;
	}

	fd_install(fd, newfile);
	r = fd;
		
out:
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);

	return r;

err_cancel:
	rpc_cancel(desc);
	rpc_end(desc, 0);
out_put_fd:
	put_unused_fd(fd);
	goto out;

err_close_faf_file:
	fput(newfile);
	goto out_put_fd;
};

long kcb_faf_getsockname (struct file * file,
			  struct sockaddr __user *usockaddr,
			  int __user *usockaddr_len)
{
	faf_client_data_t *data = file->private_data;
	faf_bind_msg_t msg;
	char sa[MAX_SOCK_ADDR];
	int sa_len;
	struct rpc_desc *desc;
	int r = -EFAULT;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	msg.server_fd = data->server_fd;
	if (get_user(msg.addrlen, usockaddr_len))
		goto out;

	desc = rpc_begin(RPC_FAF_GETSOCKNAME, data->server_id);
	rpc_pack_type(desc, msg);

	rpc_unpack_type(desc, r);
	rpc_unpack_type(desc, sa_len);
	rpc_unpack(desc, 0, &sa, sa_len);
	rpc_end(desc, 0);

	if (!r)
		r = move_addr_to_user(&sa, sa_len, usockaddr, usockaddr_len);
		
out:
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
};



long kcb_faf_getpeername (struct file * file,
			  struct sockaddr __user *usockaddr,
			  int __user *usockaddr_len)
{
	faf_client_data_t *data = file->private_data;
	faf_bind_msg_t msg;
	char sa[MAX_SOCK_ADDR];
	int sa_len;
	struct rpc_desc *desc;
	int r;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	msg.server_fd = data->server_fd;

	if (get_user(msg.addrlen, usockaddr_len))
		return -EFAULT;

	desc = rpc_begin(RPC_FAF_GETPEERNAME, data->server_id);
	rpc_pack_type(desc, msg);
	rpc_unpack_type(desc, r);
	rpc_unpack_type(desc, sa_len);
	rpc_unpack(desc, 0, &sa, sa_len);
	rpc_end(desc, 0);

	if (!r)
		r = move_addr_to_user(&sa, sa_len, usockaddr, usockaddr_len);

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
};



long kcb_faf_send (struct file * file,
		   void __user * _buff,
		   size_t len,
		   unsigned flags)
{
	faf_client_data_t *data = file->private_data;
	faf_send_msg_t msg;
	int r;
	void *buff;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	buff = vmalloc(len);
	if (!buff) {
		r = -ENOMEM;
		goto out;
	}

	if(!copy_from_user(buff, _buff, len)){
		struct rpc_desc* desc;
		
		msg.server_fd = data->server_fd;

		msg.flags = flags;
		msg.len = len;

		desc = rpc_begin(RPC_FAF_SEND, data->server_id);
		rpc_pack_type(desc, msg);
		rpc_pack(desc, 0, buff, len);
		
		rpc_unpack_type(desc, r);
		rpc_end(desc, 0);
	} else {
		r = -EFAULT;
	}

	vfree(buff);

out:
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);

	return r;
};



long kcb_faf_sendto (struct file * file,
		     void __user * _buff,
		     size_t len,
		     unsigned flags,
		     struct sockaddr __user *addr,
		     int addr_len)
{
	faf_client_data_t *data = file->private_data;
	faf_sendto_msg_t msg;
	void *buff;
	int r;
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	if (addr) {
		r = move_addr_to_kernel(addr, addr_len, &msg.sa);
		if (r)
			goto out;
		msg.addrlen = addr_len;
	} else {
		msg.addrlen = 0;
	}

	buff = vmalloc(len);
	if (!buff) {
		r = -ENOMEM;
		goto out;
	}

	if(!copy_from_user(buff, _buff, len)){
		struct rpc_desc* desc;
		
		msg.server_fd = data->server_fd;
		msg.len = len;
		msg.flags = flags;

		desc = rpc_begin(RPC_FAF_SENDTO, data->server_id);
		rpc_pack_type(desc, msg);
		rpc_pack(desc, 0, buff, len);

		rpc_unpack_type(desc, r);
		rpc_end(desc, 0);
		
	} else {
		r = -EFAULT;
	}

	vfree(buff);

out:
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
};



long kcb_faf_recv (struct file * file,
		   void __user * ubuf,
		   size_t size,
		   unsigned flags)
{
	faf_client_data_t *data = file->private_data;
	faf_send_msg_t msg;
	int r;
	void *buff;
	struct rpc_desc* desc;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	buff = vmalloc(size);
	if(!buff)
		return -ENOMEM;
	
	msg.server_fd = data->server_fd;
	msg.len = size;
	msg.flags = flags;

	desc = rpc_begin(RPC_FAF_RECV, data->server_id);
	rpc_pack_type (desc, msg);
	rpc_unpack_type(desc, r);
	
	if(r > 0){
		rpc_unpack(desc, 0, buff, r);

		if (copy_to_user(ubuf, buff, r))
			r = -EFAULT;
	};
	rpc_end(desc, 0);

	vfree(buff);
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);

	return r;
};



long kcb_faf_recvfrom (struct file * file,
		       void __user * ubuf,
		       size_t size,
		       unsigned flags,
		       struct sockaddr __user *addr,
		       int __user *addr_len)
{
	faf_client_data_t *data = file->private_data;
	faf_sendto_msg_t msg;
	int r;
	void *buff;
	char sa[MAX_SOCK_ADDR];
	int sa_len;
	struct rpc_desc* desc;
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	r = -EFAULT;
	if (addr) {
		if (get_user(msg.addrlen, addr_len))
			goto out;
	} else {
		msg.addrlen = 0;
	}

	buff = vmalloc(size);
	if (!buff) {
		r = -ENOMEM;
		goto out;
	}
	
	msg.server_fd = data->server_fd;
	msg.len = size;
	msg.flags = flags;

	desc = rpc_begin(RPC_FAF_RECVFROM, data->server_id);
	rpc_pack_type (desc, msg);
	rpc_unpack_type(desc, r);

	if(r > 0)
		rpc_unpack(desc, 0, buff, r);

	if(r >= 0){
		rpc_unpack_type(desc, sa_len);
		rpc_unpack(desc, 0, &sa, sa_len);
	};
	rpc_end(desc, 0);
	
	if (r < 0)
		goto out_free;

	if (addr) {
		int err;

		err = move_addr_to_user(&sa, sa_len, addr, addr_len);
		if (err) {
			r = err;
			goto out_free;
		}
	}
	if (copy_to_user(ubuf, buff, r))
		r = -EFAULT;

out_free:
	vfree(buff);
	
out:
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
};



long kcb_faf_shutdown (struct file * file,
		       int how)
{
	faf_client_data_t *data = file->private_data;
	faf_shutdown_msg_t msg ;
	int r;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	msg.server_fd = data->server_fd;

	msg.how = how;

	r = rpc_sync(RPC_FAF_SHUTDOWN, data->server_id, &msg, sizeof(msg));

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
};



long kcb_faf_setsockopt (struct file * file,
			 int level,
			 int optname,
			 char __user *optval,
			 int optlen)
{
	faf_client_data_t *data = file->private_data;
	faf_setsockopt_msg_t msg;
	struct rpc_desc *desc;
	int r, err;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	msg.server_fd = data->server_fd;

	msg.level = level;
	msg.optname = optname;
	msg.optval = optval;
	msg.optlen = optlen;

	desc = rpc_begin(RPC_FAF_SETSOCKOPT, data->server_id);
	if (!desc) {
		r = -ENOMEM;
		goto out;
	}

	err = rpc_pack_type(desc, msg);
	if (err)
		goto err_cancel;
	err = handle_ruaccess(desc);
	if (err)
		goto err_cancel;
	err = rpc_unpack_type(desc, r);
	if (err)
		goto err_cancel;

out_end:
	rpc_end(desc, 0);

out:
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);

	return r;

err_cancel:
	rpc_cancel(desc);
	if (err > 0)
		err = -ENOMEM;
	r = err;
	goto out_end;
}



long kcb_faf_getsockopt (struct file * file,
			 int level,
			 int optname,
			 char __user *optval,
			 int __user *optlen)
{
	faf_client_data_t *data = file->private_data;
	faf_getsockopt_msg_t msg;
	int r, err;
	struct rpc_desc *desc;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	msg.server_fd = data->server_fd;

	msg.level = level;
	msg.optname = optname;
	msg.optval = optval;
	msg.optlen = optlen;

	desc = rpc_begin(RPC_FAF_GETSOCKOPT, data->server_id);
	if (!desc) {
		r = -ENOMEM;
		goto out;
	}

	err = rpc_pack_type(desc, msg);
	if (err)
		goto err_cancel;
	err = handle_ruaccess(desc);
	if (err)
		goto err_cancel;
	err = rpc_unpack_type(desc, r);
	if (err)
		goto err_cancel;

out_end:
	rpc_end(desc, 0);

out:
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);

	return r;

err_cancel:
	rpc_cancel(desc);
	if (err > 0)
		err = -ENOMEM;
	r = err;
	goto out_end;
}



long kcb_faf_sendmsg (struct file * file,
		      struct msghdr __user *msghdr,
		      unsigned flags)
{
	faf_client_data_t *data = file->private_data;
	faf_sendmsg_msg_t msg;
	int r;
	struct rpc_desc* desc;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	msg.server_fd = data->server_fd;

	msg.flags = flags;

	desc = rpc_begin(RPC_FAF_SENDMSG, data->server_id);
	rpc_pack_type(desc, msg);

	send_msghdr(desc, msghdr, 1);

	rpc_unpack_type(desc, r);
	rpc_end(desc, 0);
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);

	return r;
};



long kcb_faf_recvmsg(struct file * file,
		     struct msghdr __user *msghdr,
		     unsigned int flags)
{
	faf_client_data_t *data = file->private_data;
	faf_sendmsg_msg_t msg;
	int r;
	struct rpc_desc* desc;

	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_ENTER, 0);

	msg.server_fd = data->server_fd;

	msg.flags = flags;

	desc = rpc_begin(RPC_FAF_RECVMSG, data->server_id);
	rpc_pack_type(desc, msg);

	send_msghdr(desc, msghdr, 1);

	rpc_unpack_type(desc, r);
	recv_msghdr(desc, msghdr, 1);

	rpc_end(desc, 0);
	
	DEBUG ("server", 2, -1,data->server_id, data->server_fd, file->f_objid,
	       NULL, file, FAF_LOG_EXIT, r);
	
	return r;
};



char *kcb_faf_d_path(struct file *file, char *buffer, int size)
{
	return faf_d_path (file, buffer, size);
}



void kcb_faf_srv_close(struct file *file)
{
	check_close_faf_srv_file(file);
}



static int kcb_faf_poll_wait(struct file *file, int wait)
{
	faf_client_data_t *data = file->private_data;
	struct faf_poll_wait_msg msg;
	struct rpc_desc *desc;
	unsigned int revents;
	int err = -ENOMEM, res = 0;
	long old_state = current->state;

	data->poll_revents = 0;

	msg.server_fd = data->server_fd;
	msg.objid = file->f_objid;
	msg.wait = wait;

	desc = rpc_begin(RPC_FAF_POLL_WAIT, data->server_id);
	if (!desc)
		goto out;
	err = rpc_pack_type(desc, msg);
	if (err)
		goto err_cancel;
	if (wait) {
		err = rpc_unpack_type(desc, res);
		if (err)
			goto err_cancel;
	}
	err = rpc_unpack_type(desc, revents);
	if (err)
		goto err_cancel;

	if (res)
		err = res;
	data->poll_revents = revents;

out_end:
	rpc_end(desc, 0);

out:
	/*
	 * after sleeping rpc_unpack() returns with
	 * current->state == TASK_RUNNING
	 */
	set_current_state(old_state);
	return err;

err_cancel:
	rpc_cancel(desc);
	if (err > 0)
		err = -ENOMEM;
	goto out_end;
}

static void kcb_faf_poll_dequeue(struct file *file)
{
	faf_client_data_t *data = file->private_data;
	struct faf_notify_msg msg;
	int err;

	msg.server_fd = data->server_fd;
	msg.objid = file->f_objid;
	err = rpc_async(RPC_FAF_POLL_DEQUEUE, data->server_id,
			&msg, sizeof(msg));
	if (err)
		printk("faf_poll: memory leak on server %d!\n", data->server_id);
}

/** Kerrighed kernel hook for FAF poll function.
 *  @author Renaud Lottiaux
 * 
 *  @param file          File to do a poll to.
 */
unsigned int faf_poll (struct file *file,
		       struct poll_table_struct *wait)
{
	faf_client_data_t *data = file->private_data;
	unsigned int revents;
	long old_state = current->state;

	mutex_lock(&faf_poll_mutex);
	/* Waking up from mutex_lock() sets current->state to TASK_RUNNING */
	set_current_state(old_state);
	poll_wait(file, &data->poll_wq, wait);
	if (!wait)
		kcb_faf_poll_wait(file, 0);
	revents = data->poll_revents;
	mutex_unlock(&faf_poll_mutex);

	return revents;
}

static void handle_faf_poll_notify(struct rpc_desc *desc,
				   void *_msg,
				   size_t size)
{
	unsigned long dvfs_id = *(unsigned long *)_msg;
	struct dvfs_file_struct *dvfs_file;
	faf_client_data_t *data;

	dvfs_file = _kddm_get_object_no_ft(dvfs_file_struct_ctnr, dvfs_id);
	if (dvfs_file && dvfs_file->file) {
		/* TODO: still required? */
		if (atomic_read (&dvfs_file->file->f_count) == 0)
			dvfs_file->file = NULL;
	}
	if (!dvfs_file || !dvfs_file->file)
		goto out_put_dvfs_file;

	data = dvfs_file->file->private_data;
	wake_up_interruptible_all(&data->poll_wq);

out_put_dvfs_file:
	_kddm_put_object(dvfs_file_struct_ctnr, dvfs_id);
}


struct file_operations faf_file_ops = {
	poll: faf_poll,
};



/* FAF Hooks Initialisation */

void faf_hooks_init (void)
{
	hook_register(&kh_faf_lseek, kcb_faf_lseek);
	hook_register(&kh_faf_llseek, kcb_faf_llseek);
	hook_register(&kh_faf_read, kcb_faf_read);
	hook_register(&kh_faf_write, kcb_faf_write);
	hook_register(&kh_faf_ioctl, kcb_faf_ioctl);
	hook_register(&kh_faf_fcntl, kcb_faf_fcntl);
#if BITS_PER_LONG == 32
	hook_register(&kh_faf_fcntl64, kcb_faf_fcntl64);
#endif
	hook_register(&kh_faf_fstat, kcb_faf_fstat);
	hook_register(&kh_faf_fsync, kcb_faf_fsync);
	hook_register(&kh_faf_flock, kcb_faf_flock);
	hook_register(&kh_faf_bind, kcb_faf_bind);
	hook_register(&kh_faf_connect, kcb_faf_connect);
	hook_register(&kh_faf_listen, kcb_faf_listen);
	hook_register(&kh_faf_accept, kcb_faf_accept);
	hook_register(&kh_faf_getsockname, kcb_faf_getsockname);
	hook_register(&kh_faf_getpeername, kcb_faf_getpeername);
	hook_register(&kh_faf_send, kcb_faf_send);
	hook_register(&kh_faf_sendto, kcb_faf_sendto);
	hook_register(&kh_faf_recv, kcb_faf_recv);
	hook_register(&kh_faf_recvfrom, kcb_faf_recvfrom);
	hook_register(&kh_faf_shutdown, kcb_faf_shutdown);
	hook_register(&kh_faf_setsockopt, kcb_faf_setsockopt);
	hook_register(&kh_faf_getsockopt, kcb_faf_getsockopt);
	hook_register(&kh_faf_sendmsg, kcb_faf_sendmsg);
	hook_register(&kh_faf_recvmsg, kcb_faf_recvmsg);  
	hook_register(&kh_faf_d_path, kcb_faf_d_path);  
	hook_register(&kh_faf_srv_close, kcb_faf_srv_close);
	hook_register(&kh_faf_poll_wait, kcb_faf_poll_wait);
	hook_register(&kh_faf_poll_dequeue, kcb_faf_poll_dequeue);

	rpc_register_void(RPC_FAF_POLL_NOTIFY, handle_faf_poll_notify, 0);
}



/* FAF Hooks Finalization */

void faf_hooks_finalize (void)
{
}
