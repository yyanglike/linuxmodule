#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/wait.h>
#include <net/sock.h>

#include <linux/cdev.h>
#include <linux/device.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define QUEUE_SIZE 100
#define BUFFER_SIZE 1024
#define CLASS "chrdev"

static struct task_struct *send_thread;
static struct socket *conn_socket;
static DECLARE_WAIT_QUEUE_HEAD(wq);
static DEFINE_MUTEX(queue_lock);
static struct sk_buff_head data_queue;
static struct sockaddr_in server_addr;

static int thread_data = 0;  // 线程数据

static int Major,devOC=0;
static struct class* dev_class;
static struct device* dev;


static  DEFINE_MUTEX(dev_mutex);
static char msg[256],*msgPtr;
static bool stop_thread = false;
module_param(stop_thread, bool, 0644);
MODULE_PARM_DESC(stop_thread, "Stop the send thread");

#define DEVICE_NAME "my_module_device"

static int enqueue_data(const char *data, size_t length)
{
    struct sk_buff *skb;

    skb = alloc_skb(length, GFP_KERNEL);
    if (!skb) {
        printk(KERN_ALERT "bad alloc skb");
        return -ENOMEM;
    }

    skb_put_data(skb, data, length);

    mutex_lock(&queue_lock);
    skb_queue_tail(&data_queue, skb);
    mutex_unlock(&queue_lock);

    wake_up(&wq);

    return 0;
}
static int my_module_open(struct inode* inode,struct file* file)
{
    if(!mutex_trylock(&dev_mutex))
    {
        printk(KERN_ALERT"Device Busy!");
        return -EBUSY;
    }
    devOC++;
    msgPtr=msg;
    return 0;
}
static int my_module_close(struct inode* inode,struct file* file)
{
    devOC--;
    printk(KERN_INFO"Device Closed: %d",devOC);
    mutex_unlock(&dev_mutex);
    return 0;
}
static ssize_t my_module_read(struct file* file,char* buff,size_t length,loff_t* offset)
{
    int bytes_read=0;
    while(*msgPtr&&length--)
    {
        put_user(*(msgPtr++),buff++);
        bytes_read++;
    }
    return bytes_read++;
}
static ssize_t my_module_write(struct file* file,const char*buff,size_t length,loff_t *offset)
{
    // copy_from_user(msg,buff,length);
    // printk("Data Recieved: %s",msg);
    // msgPtr=msg;
    // // enqueue(msg,length);
    // return length;

    char *data;
    int result;

    data = kmalloc(length, GFP_KERNEL);
    if (!data) {
        printk(KERN_ALERT "Failed to allocate memory for data\n");
        return -ENOMEM;
    }

    if (copy_from_user(data, buff, length)) {
        printk(KERN_ALERT "Failed to copy data from user\n");
        kfree(data);
        return -EFAULT;
    }

    result = enqueue_data(data, length);
    if (result < 0) {
        printk(KERN_ALERT "Failed to enqueue data\n");
    }
    return length;
}

static struct file_operations fops={
.owner=THIS_MODULE,
.read=my_module_read,
.release=my_module_close,
.open=my_module_open,
.write=my_module_write,
};



static int send_data(void* data)
{
    struct sk_buff *skb;
    int err = 0;
    int reconnect_count = 0;

    const char *data1 = "Hello, World!";
    size_t length1 = strlen(data);

    int thread_data = *((int*)data);
    unsigned long timeout = msecs_to_jiffies(5000);  // 超时时间为 5000 毫秒
    // printk(KERN_INFO "%d",thread_data);
    printk(KERN_INFO "Thread data: %d\n", thread_data);

    while (!kthread_should_stop()) {
        if (!conn_socket) {
            // 创建TCP连接
            conn_socket = NULL;
            err = sock_create_kern(AF_INET, SOCK_STREAM, 0, &conn_socket);
            if (err < 0) {
                printk(KERN_ALERT "Failed to create socket\n");
                continue;
            }

            // 设置服务器地址和端口
            memset(&server_addr, 0, sizeof(struct sockaddr_in));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(SERVER_PORT);
            server_addr.sin_addr.s_addr = in_aton(SERVER_IP);

            // 尝试连接外部服务器
            err = kernel_connect(conn_socket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in), 0);
            if (err < 0) {
                printk(KERN_ALERT "Failed to connect to server\n");
                sock_release(conn_socket);
                conn_socket = NULL;
                msleep(1000);  // 等待重新连接
                reconnect_count++;
                continue;
            }

            reconnect_count = 0;
        }

        if(stop_thread){
            break;
        }
        
        // 等待队列非空
        // wait_event_interruptible(wq, !skb_queue_empty(&data_queue));

        // 等待队列超时或者队列不为空

        if (wait_event_interruptible_timeout(wq, !skb_queue_empty(&data_queue), timeout) < 0) {
            // 等待超时，执行相应处理逻辑
            // ...
            printk(KERN_INFO "wait queue timeout\n");
            continue;
        } else {
            // printk(KERN_INFO "Event occurred before timeout\n");
            // 执行事件发生处理逻辑
            //  enqueue_data(data1, length1);
        }

        // 从队列中获取数据
        mutex_lock(&queue_lock);
        skb = skb_dequeue(&data_queue);
        mutex_unlock(&queue_lock);

        // 发送数据到外部服务器
        if (skb) {
            struct msghdr msg;
            struct kvec iov;

            memset(&msg, 0, sizeof(struct msghdr));
            msg.msg_name = (struct sockaddr *)&server_addr;
            msg.msg_namelen = sizeof(struct sockaddr_in);

            iov.iov_base = skb->data;
            iov.iov_len = skb->len;

            err = kernel_sendmsg(conn_socket, &msg, &iov, 1, skb->len);
            printk(KERN_INFO "send data to server :%s\n", skb->data);

            kfree_skb(skb);
        }

        // 发送失败，断开连接并重新连接
        if (err < 0) {
            if (err == -EPIPE || err == -ECONNRESET) {
                printk(KERN_ALERT "Connection lost, reconnecting...\n");
                kernel_sock_shutdown(conn_socket, SHUT_RDWR);
                sock_release(conn_socket);
                conn_socket = NULL;
                msleep(1000);  // 等待重新连接
                reconnect_count++;
                continue;
            } else {
                // 发送出错，处理错误情况
                // ...
            }
        }

        // 连续多次连接失败，可能出现问题，放慢重连速度
        if (reconnect_count > 5) {
            msleep(5000);  // 等待重新连接
        }
    }

    return 0;
}


// // 设备文件操作函数
// static ssize_t my_module_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
// {
//     char *data;
//     int result;

//     data = kmalloc(count, GFP_KERNEL);
//     if (!data) {
//         printk(KERN_ALERT "Failed to allocate memory for data\n");
//         return -ENOMEM;
//     }

//     if (copy_from_user(data, buf, count)) {
//         printk(KERN_ALERT "Failed to copy data from user\n");
//         kfree(data);
//         return -EFAULT;
//     }

//     result = enqueue_data(data, count);
//     if (result < 0) {
//         printk(KERN_ALERT "Failed to enqueue data\n");
//     }

//     kfree(data);

//     return count;
// }

// static struct file_operations my_module_fops = {
//     .owner = THIS_MODULE,
//     .write = my_module_write,
// };

static int __init module_init_function(void)
{
    int err;
    const char *data = "Hello, World!";
    size_t length = strlen(data);
        // 创建字符设备结构体
    // 创建字符设备结构体
    struct cdev my_module_cdev;
    // 创建设备结构体
    struct device *my_device = NULL;


    // 初始化队列和锁
    skb_queue_head_init(&data_queue);
    mutex_init(&queue_lock);


    // 调用enqueue_data函数
    enqueue_data(data, length);
        // 创建后台线程
    send_thread = kthread_create(send_data, (void *)&thread_data, "send_thread");
    if (IS_ERR(send_thread)) {
        printk(KERN_ALERT "Failed to create send thread\n");
        return PTR_ERR(send_thread);
    }

        // 启动后台线程
    wake_up_process(send_thread);

    // // 创建设备文件
    // if (register_chrdev(0, DEVICE_NAME, &my_module_fops) < 0) {
    //     printk(KERN_ALERT "Failed to register device\n");
    //     return -1;
    // }    


    Major=register_chrdev(0,DEVICE_NAME,&fops);
    if(Major<0)
    {
        printk(KERN_ALERT"Error Registering Device");
        return -EPERM;
    }
    dev_class=class_create(THIS_MODULE,CLASS);
    if(IS_ERR(dev))
    {
        printk(KERN_ALERT"Error Creating Device Class!");
        unregister_chrdev(Major,DEVICE_NAME);
        return PTR_ERR(dev);
    }
    dev=device_create(dev_class,NULL,MKDEV(Major,0),NULL,DEVICE_NAME);
    if(IS_ERR(dev))
    {
        printk(KERN_ALERT"Error Creating Device");
        class_destroy(dev_class);
        unregister_chrdev(Major,DEVICE_NAME);
        return PTR_ERR(dev);
    }

    mutex_init(&dev_mutex);
    printk(KERN_INFO "Device file registered\n");


    // 创建TCP连接
    conn_socket = NULL;
    err = sock_create_kern(AF_INET, SOCK_STREAM, 0, &conn_socket);
    if (err < 0) {
        printk(KERN_ALERT "Failed to create socket\n");
        return err;
    }

    // 设置服务器地址和端口
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = in_aton(SERVER_IP);


    printk(KERN_INFO "Module loaded\n");
    return 0;
}

static void __exit module_exit_function(void)
{
    stop_thread = true;
    // 停止发送线程
    if (send_thread) {
        printk(KERN_INFO "before stop thread\n");
        kthread_stop(send_thread);
        printk(KERN_INFO "Stop the thread!\n");
        send_thread = NULL;
    }

    // 释放队列中的数据
    mutex_lock(&queue_lock);
    while (!skb_queue_empty(&data_queue)) {
        struct sk_buff *skb = skb_dequeue(&data_queue);
        kfree_skb(skb);
    }
    mutex_unlock(&queue_lock);

    printk(KERN_INFO "empth queue\n");
    // 释放连接资源
    if (conn_socket) {
        kernel_sock_shutdown(conn_socket, SHUT_RDWR);
        sock_release(conn_socket);
        conn_socket = NULL;
        printk(KERN_INFO "shutdown socket\n");
    }

    mutex_destroy(&dev_mutex);
    device_destroy(dev_class,MKDEV(Major,0));
    class_unregister(dev_class);
    class_destroy(dev_class);
    unregister_chrdev(Major,DEVICE_NAME);
    printk(KERN_INFO"Clean Up Done!");
        printk(KERN_INFO "Module unloaded\n");
    }



module_init(module_init_function);
module_exit(module_exit_function);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Your module description");
