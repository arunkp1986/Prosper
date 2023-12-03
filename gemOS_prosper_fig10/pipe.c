#include<pipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
/**
 * Initalizing the pipe info;
 */
struct pipe_info* alloc_pipe_info()
{
    struct pipe_info *pipe = (struct pipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);
    pipe->read_pos = -1;
    pipe->write_pos = 0;
    pipe->pipe_buff = buffer;
    pipe->is_ropen = 1;
    pipe->is_wopen = 1;

    return pipe;
}

long pipe_close(struct file *filep)
{
    int ret_fd = -EINVAL; 
        // If the type is pipe then check the reference count before de allocating the pages;
     struct pipe_info *p_info = filep->pipe;
     if(filep->ref_count == 1 && p_info)
        {
            if(filep->mode == O_READ)
                p_info->is_ropen = 0;
            else 
                p_info->is_wopen = 0;

            if(!(p_info->is_ropen  || p_info->is_wopen ))
                os_page_free(OS_DS_REG, p_info);
        }
    ret_fd = std_close(filep);
    return ret_fd;
}


/**
 * Validation function for read and write system call
 */
u8 validate_request(struct file *filep, u32 count, u8 mode)
{ 
    int retval = 0;
    if(count > PIPE_MAX_SIZE ||filep== NULL || filep->mode != mode || filep->pipe == NULL)
    {
        retval =  1;
    }
    return retval;
}

/**
 * Circular queue implementation for Read and write pipe
 */
int process_buffer(struct pipe_info *pipe_info, int mode, char* buff, u32 count)
{
    int bytes_processed = 0;
    int *offset, *check_off;
    // Based on the mode get the offset and check values
    if(mode == O_READ)
    {
        offset = &pipe_info->read_pos;
        check_off =  &pipe_info->write_pos;
    } else
    {
        offset = &pipe_info->write_pos;
        check_off =  &pipe_info->read_pos;
    }
    while(bytes_processed < count && (*offset)!= (*check_off))
    {
        //Incase of read mode read into the user buffer
        // Else read from user buffer and write into the pipe
        if(mode == O_READ)
        {
            // Reseting the initial Read offest value to 0
             (*offset) = (*offset) == -1 ? 0 : (*offset);
             buff[bytes_processed] = pipe_info->pipe_buff[pipe_info -> read_pos];
        } else
        {
            pipe_info->pipe_buff[pipe_info -> write_pos] = buff[bytes_processed];
        }
        (*offset)++;

        //Reseting the index to zero
        if( (*offset) >= PIPE_MAX_SIZE)
        {
             (*offset) = 0;
        }
        bytes_processed++;
    }
    // Calculating the current buffer length of the pipe
    int read_pos = pipe_info->read_pos == -1 ? 0 : pipe_info->read_pos;
    if(read_pos < pipe_info->write_pos)
    {
        pipe_info->buffer_offset = pipe_info->write_pos - read_pos;
    } else if (read_pos > pipe_info->write_pos)
    {
        pipe_info->buffer_offset = (PIPE_MAX_SIZE - (read_pos - pipe_info->write_pos));
    } else
    {
        pipe_info->buffer_offset  = 0;
    }
    
    if(mode == O_READ && (*offset)== (*check_off))
    {
        pipe_info->read_pos = -1;
        pipe_info->write_pos = 0;
    }
    return bytes_processed;
}

int pipe_read(struct file *filep, char *buff, u32 count)
{
    /**
    *  TODO:: Implementation of Pipe Read
    *  Read the contect from buff (pipe_info -> pipe_buff) and write to the buff(argument 2);
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */

    if(filep->mode != O_READ){  // Return error if not mode is not correct
        return -EACCES;
    }

    int buffer_offset = filep->pipe->buffer_offset;
    // If we are reading more than what is present in file it should return error
    if(count > buffer_offset){
        return -EINVAL;
    }

    char *buff1 = filep->pipe->pipe_buff;
    // Implement reading as a circular queue
    int read_pos = filep->pipe->read_pos;
    int write_pos = filep->pipe->write_pos;
    int size = PIPE_MAX_SIZE;

    for(int i=0;i<count;i++){
        if(read_pos == -1){
            return -EINVAL;
        }
        buff[i] = buff1[read_pos];
        if(read_pos == write_pos){
            read_pos  = -1;
            write_pos = -1;
        } else if(read_pos == size-1){
            read_pos = 0;
        } else {
            read_pos++;
        }
    }
    buffer_offset -= count;
    filep->pipe->buffer_offset = buffer_offset;
    filep->pipe->read_pos = read_pos;
    filep->pipe->write_pos = write_pos;
    return count;
}


int pipe_write(struct file *filep, char *buff, u32 count)
{
    /**
    *  TODO:: Implementation of Pipe Read
    *  Write the contect from   the buff(argument 2);  and write to buff(pipe_info -> pipe_buff)
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */

    if(filep->mode != O_WRITE){ // Return error if not mode is not correct
        return -EACCES;
    }

    int buffer_offset = filep->pipe->buffer_offset;
    // If after writing size becomes more than 4KB return error.
    if(buffer_offset + count > PIPE_MAX_SIZE){
        return -EINVAL;
    }

    char *buff1 = filep->pipe->pipe_buff;
    int read_pos = filep->pipe->read_pos;
    int write_pos = filep->pipe->write_pos;
    int size = PIPE_MAX_SIZE;
    // Implement writing as a circular queue
    for(int i=0;i<count;i++){
        if((read_pos == 0 && write_pos == size-1) || (write_pos == (read_pos-1)%(size-1))){
            return -EINVAL;
        } else if(read_pos == -1){
            read_pos = write_pos = 0;
            buff1[write_pos] = buff[i];
        } else if(write_pos == size-1 && read_pos != 0){
            write_pos = 0;
            buff1[write_pos] = buff[i];
        } else {
            write_pos++;
            buff1[write_pos] = buff[i];
        }
    }
    buffer_offset += count;
    filep->pipe->buffer_offset = buffer_offset; 
    filep->pipe->write_pos = write_pos;
    filep->pipe->read_pos = read_pos;
    return count;
}

int create_pipe(struct exec_context *current, int *fd)
{
    /**
    *  TODO:: Implementation of Pipe Create
    *  Create file struct by invoking the alloc_file() function, 
    *  Create pipe_info struct by invoking the alloc_pipe_info() function
    *  fill the valid file descriptor in *fd param
    *  Incase of Error return valid Error code 
    */
    int fd1 = 0;
    // search for fds for read and write end
    while(current->files[fd1]){
        fd1++;
    }
    if(fd1 >= MAX_OPEN_FILES){
        return -EINVAL;
    }
    fd[0] = fd1++;;
    while(current->files[fd1]){
        fd1++;
    }
    if(fd1 >= MAX_OPEN_FILES){
        return -EINVAL;
    }
    fd[1] = fd1;
    struct file *filep1 = alloc_file();
    if(filep1 == NULL) {
        return -ENOMEM;
    }
    struct file *filep2 = alloc_file();
    if(filep2 == NULL) {
        return -ENOMEM;
    }
    current->files[fd[0]] = filep1;
    current->files[fd[1]] = filep2;
    struct pipe_info *pipe_info_p = alloc_pipe_info();
    if(pipe_info_p == NULL) {
        return -ENOMEM;
    }
    // both file structs share same pipe info
    filep1->pipe = pipe_info_p;
    filep2->pipe = pipe_info_p;
    filep1->mode = O_READ; // read end of pipe
    filep2->mode = O_WRITE; // write end of pipe
    // assign read and write functions for file structs
    filep1->fops->read = pipe_read;
    filep1->fops->write = NULL;
    filep1->fops->close = pipe_close;
    filep2->fops->write = pipe_write;
    filep2->fops->read = NULL;
    filep2->fops->close = pipe_close;
    filep1->ref_count = 1;
    filep2->ref_count = 1;
    // initialize variables for pipe
    pipe_info_p->read_pos = -1;
    pipe_info_p->write_pos = -1;
    pipe_info_p->buffer_offset = 0;
    pipe_info_p->is_ropen = 1;
    pipe_info_p->is_wopen = 1;
    return 0;
}

