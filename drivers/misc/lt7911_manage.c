/*
 * change log :
 * 0.0.1  - based on LT6911C code for LT7911D
 * 0.0.2  - remove LT86102 all operations
 * 0.0.3  - fix hactive value read and add fps detection
 * 0.0.4  - support dt parse for GPIO and I2C
 *
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include "lt7911_manage.h"

static struct work_struct get_video_info_work;
static bool lt7911_init_done;

static int force_width = -1;
static int force_height = -1;
static int force_fps = -1;

void lt7911_force_resolution(u16 width, u16 height, int fps);

static int force_res_param_set(const char *val, const struct kernel_param *kp)
{
    int old_val = *(int *)kp->arg;
    int ret = param_set_int(val, kp);
    if (ret)
        return ret;

    if (old_val != -1 && *(int *)kp->arg == -1) {
        schedule_work(&get_video_info_work);
        return 0;
    }

    if (force_width > 0 && force_height > 0)
        lt7911_force_resolution((u16)force_width, (u16)force_height, force_fps);

    return 0;
}

static const struct kernel_param_ops force_res_param_ops = {
    .set = force_res_param_set,
    .get = param_get_int,
};

// module use parameters with "insmod lt7911_manage.ko force_width=1920 force_height=1080 force_fps=30"
module_param_cb(force_width, &force_res_param_ops, &force_width, 0644);
MODULE_PARM_DESC(force_width, "Force HDMI width");
module_param_cb(force_height, &force_res_param_ops, &force_height, 0644);
MODULE_PARM_DESC(force_height, "Force HDMI height");
module_param_cb(force_fps, &force_res_param_ops, &force_fps, 0644);
MODULE_PARM_DESC(force_fps, "Force HDMI fps");

static int irq_number;
static struct i2c_client *client;
static int i2c_bus_num = -1;
static u8 lt7911_i2c_addr;
static bool dt_cfg_valid;

enum {
    LT7911_CHIP_UNKNOWN,
    LT7911_CHIP_LT7911D,
} typedef chip_platform_t;

static chip_platform_t chip_platform = LT7911_CHIP_UNKNOWN;

static int INT_PIN;
static int PWR_PIN;

static u8 old_offset = 0xff;

u16 hdmi_res_list[][2] = {
    {3840, 2400},
    {3840, 2160},
    {3440, 1440},
    {2560, 1600},
    {2560, 1440},
    {2560, 1080},
    {2048, 1536},
    {2048, 1152},
    {1920, 1440},
    {1920, 1200},
    {1920, 1080},
    {1680, 1050},
    {1600, 1200},
    {1600, 900},
    {1440, 1080},
    {1440, 900},
    {1440, 1050},
    {1368, 768},
    {1280, 1024},
    {1280, 960},
    {1280, 800},
    {1280, 720},
    {1152, 864},
    {1024, 768},
    {800, 600},
};

u16 hdmi_unsupported_res_list[][2] = {
    {1366, 768},
};

static struct proc_dir_entry *proc_lt7911_dir;
static struct proc_dir_entry *proc_video_status_file;
static struct proc_dir_entry *proc_video_width_file;
static struct proc_dir_entry *proc_video_height_file;
static struct proc_dir_entry *proc_video_power_file;
static struct proc_dir_entry *proc_video_fps_file;
static struct proc_dir_entry *proc_video_hdcp_file;
static struct proc_dir_entry *proc_audio_sample_rate_file;
static struct proc_dir_entry *proc_video_edid_file;
static struct proc_dir_entry *proc_video_edid_snapshot_file;
static struct proc_dir_entry *proc_version_file;
static char video_status_buffer[16];
static char video_status_write_buffer[16];
static char video_width_buffer[16];
static char vidio_height_buffer[16];
static char video_power_buffer[16];
static char video_power_write_buffer[16];
static char video_fps_buffer[16];
static char video_hdcp_buffer[16];
static char audio_sample_rate_buffer[16];
static char video_edid_buffer[256];
static char video_edid_snapshot_buffer[256];
static char version_buffer[32] = {0};

static int video_status_buffer_length;
static int video_width_buffer_length;
static int video_height_buffer_length;
static int video_power_buffer_length;
static int video_power_write_buffer_length;
static int video_fps_buffer_length;
static int video_hdcp_buffer_length;
static int audio_sample_rate_buffer_length;
static int video_edid_buffer_length;
static int video_edid_snapshot_buffer_length;
static int version_buffer_length;

static irqreturn_t gpio_irq_handler(int irq, void *dev_id);
int lt7911_pwr_ctrl(int pwr_en);
int check_edid(u8 *edid_data, u16 edid_size);
int lt7911_edid_write(u8 *edid_data, u16 edid_size);
int lt7911_edid_read(u8 *edid_data, u16 edid_size);
int lt7911_str_write(u8 *str, u16 len);
int lt7911_str_read(u8 *str);

struct lt7911_event_ctx {
    wait_queue_head_t wait_queue;
    atomic_t status;
};

static struct lt7911_event_ctx lt7911_ctx = {
    .wait_queue = __WAIT_QUEUE_HEAD_INITIALIZER(lt7911_ctx.wait_queue),
    .status     = ATOMIC_INIT(0),
};

struct lt7911_priv_data {
    int last_status;
};

void update_status(void)
{
    // if buffer is :"new res"/"normal res"/"unsupport res"/"unknown res"/"error res" , buffer = stable
    if (video_status_buffer_length > 0 &&
        (strncmp(video_status_buffer, "new res", 7) == 0 ||
         strncmp(video_status_buffer, "normal res", 11) == 0 ||
         strncmp(video_status_buffer, "unsupport res", 13) == 0 ||
         strncmp(video_status_buffer, "unknown res", 12) == 0 ||
         strncmp(video_status_buffer, "error res", 9) == 0)) {
        snprintf(video_status_buffer, sizeof(video_status_buffer), "stable\n");
        video_status_buffer_length = strlen(video_status_buffer);

        atomic_inc(&lt7911_ctx.status);
        wake_up_interruptible(&lt7911_ctx.wait_queue);
        return;
    }
}

ssize_t proc_video_power_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    // printk(KERN_INFO "Reading from proc file\n");
    return simple_read_from_buffer(user_buffer, count, offset, video_power_buffer, video_power_buffer_length);
}

ssize_t proc_video_power_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{

    if (count > sizeof(video_power_write_buffer) - 1) {
        return -EINVAL; // Buffer overflow
    }

    if (copy_from_user(video_power_write_buffer, user_buffer, count)) {
        return -EFAULT; // Copy failed
    }

    video_power_write_buffer[count] = '\0'; // Null-terminate the string
    video_power_write_buffer_length = count; // Update buffer length

    // check if "on" or "off"
    if ((strncmp(video_power_write_buffer, "on", 2) == 0) || (strncmp(video_power_write_buffer, "1", 1) == 0)) {
        if (lt7911_pwr_ctrl(1) < 0) {
            return -EIO; // Power control failed
        }
        // printk(KERN_INFO "Turning HDMI power on, buffer: %s\n", hdmi_power_buffer);
    } else if ((strncmp(video_power_write_buffer, "off", 3) == 0) || (strncmp(video_power_write_buffer, "0", 1) == 0)) {
        if (lt7911_pwr_ctrl(0) < 0) {
            return -EIO; // Power control failed
        }
        // update video_status_buffer
        snprintf(video_status_buffer, sizeof(video_status_buffer), "disappear\n");
        video_status_buffer_length = strlen(video_status_buffer);

        atomic_inc(&lt7911_ctx.status);
        wake_up_interruptible(&lt7911_ctx.wait_queue);
        // printk(KERN_INFO "Turning video power off, buffer: %s\n", video_power_buffer);
    } else {
        // printk(KERN_INFO "Turning video power error\n");
        return -EINVAL; // Invalid input
    }

    return count; // Return number of bytes written
}

ssize_t proc_video_status_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    struct lt7911_priv_data *priv = file->private_data;
    ssize_t ret;

    // printk(KERN_INFO "Reading from proc file\n");
    ret = simple_read_from_buffer(user_buffer, count, offset, video_status_buffer, video_status_buffer_length);

    if (ret > 0 && priv) {
        priv->last_status = atomic_read(&lt7911_ctx.status);
    }

    return ret;
}

ssize_t proc_video_status_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
    if (count > sizeof(video_status_write_buffer) - 1) {
        return -EINVAL; // Buffer overflow
    }

    if (copy_from_user(video_status_write_buffer, user_buffer, count)) {
        return -EFAULT; // Copy failed
    }

    // check input is "ok"
    if (strncmp(video_status_write_buffer, "ok", 2) == 0) {
        update_status();
    }

    return count; // Return number of bytes written
}

unsigned int proc_video_status_poll(struct file *file, poll_table *wait)
{
    struct lt7911_priv_data *priv = file->private_data;
    unsigned int mask = 0;

    poll_wait(file, &lt7911_ctx.wait_queue, wait);

    if (!priv) return POLLERR;

    if (atomic_read(&lt7911_ctx.status) != priv->last_status) {
        mask |= POLLPRI;
    }

    return mask;
}

ssize_t proc_video_width_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    // printk(KERN_INFO "Reading from proc file\n");
    return simple_read_from_buffer(user_buffer, count, offset, video_width_buffer, video_width_buffer_length);
}

ssize_t proc_video_height_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    // printk(KERN_INFO "Reading from proc file\n");
    return simple_read_from_buffer(user_buffer, count, offset, vidio_height_buffer, video_height_buffer_length);
}

ssize_t proc_video_fps_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    // printk(KERN_INFO "Reading from proc file\n");
    return simple_read_from_buffer(user_buffer, count, offset, video_fps_buffer, video_fps_buffer_length);
}

ssize_t proc_video_hdcp_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    // printk(KERN_INFO "Reading from proc file\n");
    return simple_read_from_buffer(user_buffer, count, offset, video_hdcp_buffer, video_hdcp_buffer_length);
}

ssize_t proc_audio_sample_rate_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    // printk(KERN_INFO "Reading from proc file\n");
    return simple_read_from_buffer(user_buffer, count, offset, audio_sample_rate_buffer, audio_sample_rate_buffer_length);
}

static ktime_t last_read_time = 0;
ssize_t proc_video_edid_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    // Get the current time
    ktime_t current_time = ktime_get_real();
    s64 interval = ktime_to_ms(ktime_sub(current_time, last_read_time));

    // Check if the interval since the last read is greater than 500 ms
    if (interval > 500) {
        // Ensure that the chip is in the on state
        // Power on the LT7911UXC first
        lt7911_pwr_ctrl(1);

        // read EDID from chip
        video_edid_buffer_length = EDID_BUFFER_SIZE;
        printk(KERN_INFO "Reading EDID from LT7911UXC...\n");
        if (lt7911_edid_read(video_edid_buffer, video_edid_buffer_length) < 0) {
            return -EIO; // EDID read failed
        }

        // copy to video_edid_snapshot_buffer
        memcpy(video_edid_snapshot_buffer, video_edid_buffer, video_edid_buffer_length);
        video_edid_snapshot_buffer_length = video_edid_buffer_length;

        // restart the chip
        printk(KERN_INFO "Restarting LT7911...\n");
        lt7911_pwr_ctrl(0);
        msleep(100);
        lt7911_pwr_ctrl(1);
    }

    // Update the last read time
    last_read_time = current_time;

    return simple_read_from_buffer(user_buffer, count, offset, video_edid_buffer, video_edid_buffer_length);
}

ssize_t proc_video_edid_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
    // copy to edid_write_buffer
    int i;
    u8 edid_write_buffer[count]; // 256 bytes for EDID or +32 bytes for EDID name
    u8 edid_read_buffer[count]; // 256 bytes for EDID or +32 bytes for EDID name
    if (copy_from_user(edid_write_buffer, user_buffer, count)) {
        return -EFAULT;
    }

    // print buffer by hex
    printk(KERN_INFO "Writing EDID: ");
    for (i = 0; i < count; i++) {
        printk(KERN_CONT "%02x ", edid_write_buffer[i]);
    }
    printk(KERN_CONT "\n");

    // check edid_write_buffer
    if (check_edid(edid_write_buffer, count) < 0) {
        return -EINVAL; // Invalid EDID data
    }

    // Ensure that the chip is in the on state
    // Power on the LT7911 first
    lt7911_pwr_ctrl(1);

    // write EDID to chip
    if (lt7911_edid_write(edid_write_buffer, count) < 0) {
        return -EIO; // EDID write failed
    }

    // read EDID from chip
    if (lt7911_edid_read(edid_read_buffer, count) < 0) {
        return -EIO; // EDID read failed
    }

    // copy to video_edid_snapshot_buffer
    memcpy(video_edid_snapshot_buffer, edid_read_buffer, count);
    video_edid_snapshot_buffer_length = count;

    // check if read EDID is same as write EDID
    if (memcmp(edid_write_buffer, edid_read_buffer, count) != 0) {
        printk(KERN_ERR "EDID write verification failed\n");
        return -EIO; // EDID write verification failed
    }

    // restart the chip
    printk(KERN_INFO "Restarting LT7911UXC...\n");
    lt7911_pwr_ctrl(0);
    msleep(100);
    lt7911_pwr_ctrl(1);

    return count;
}

ssize_t proc_video_edid_snapshot_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    if (video_edid_snapshot_buffer_length == EDID_BUFFER_SIZE) {
        snprintf(video_edid_snapshot_buffer, sizeof(video_edid_snapshot_buffer), "unknown\n");
    }
    return simple_read_from_buffer(user_buffer, count, offset, video_edid_snapshot_buffer, video_edid_snapshot_buffer_length);
}

ssize_t proc_version_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    if (version_buffer[0] == '\0') {
        snprintf(version_buffer, sizeof(version_buffer), "unknown\n");
    }
    version_buffer_length = strlen(version_buffer);
    return simple_read_from_buffer(user_buffer, count, offset, version_buffer, version_buffer_length);
}

ssize_t proc_version_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
    // copy to version_write_buffer
    u8 version_write_buffer[count]; // 256 bytes for EDID or +32 bytes for EDID name
    if (copy_from_user(version_write_buffer, user_buffer, count)) {
        return -EFAULT;
    }

    // print buffer by hex
    printk(KERN_INFO "Writing version: ");
    printk(KERN_CONT "%s\n", version_write_buffer);

    // Ensure that the chip is in the on state
    // Power on the LT7911 first
    lt7911_pwr_ctrl(1);

    // write version to chip
    if (lt7911_str_write(version_write_buffer, count) < 0) {
        return -EIO; // version write failed
    }

    // read version from chip
    if (lt7911_str_read(version_buffer) < 0) {
        return -EIO; // version read failed
    }

    // check if read version is same as write version
    if (memcmp(version_write_buffer, version_buffer, count) != 0) {
        printk(KERN_ERR "version write verification failed\n");
        return -EIO; // version write verification failed
    }

    // restart the chip
    printk(KERN_INFO "Restarting LT7911UXC...\n");
    lt7911_pwr_ctrl(0);
    msleep(100);
    lt7911_pwr_ctrl(1);

    return count;
}

static int proc_open(struct inode *inode, struct file *file)
{
    struct lt7911_priv_data *priv;

    if (!try_module_get(THIS_MODULE))
        return -ENODEV;

    priv = kzalloc(sizeof(struct lt7911_priv_data), GFP_KERNEL);
    if (!priv) {
        module_put(THIS_MODULE);
        return -ENOMEM;
    }

    priv->last_status = atomic_read(&lt7911_ctx.status);

    file->private_data = priv;
    return 0;
}

static int proc_release(struct inode *inode, struct file *file)
{
    struct lt7911_priv_data *priv = file->private_data;
    if (priv)
        kfree(priv);

    module_put(THIS_MODULE);
    return 0;
}

static const struct file_operations proc_video_power_fops = {
    .owner = THIS_MODULE,
    .read = proc_video_power_read,
    .write = proc_video_power_write,
};

static const struct file_operations proc_video_status_fops = {
    .owner = THIS_MODULE,
    .open    = proc_open,
    .release = proc_release,
    .read = proc_video_status_read,
    .write = proc_video_status_write,
    .poll = proc_video_status_poll,
};

static const struct file_operations proc_video_width_fops = {
    .owner = THIS_MODULE,
    .read = proc_video_width_read,
};

static const struct file_operations proc_video_height_fops = {
    .owner = THIS_MODULE,
    .read = proc_video_height_read,
};

static const struct file_operations proc_video_fps_fops = {
    .owner = THIS_MODULE,
    .read = proc_video_fps_read,
};

static const struct file_operations proc_video_hdcp_fops = {
    .owner = THIS_MODULE,
    .read = proc_video_hdcp_read,
};

static const struct file_operations proc_audio_sample_rate_fops = {
    .owner = THIS_MODULE,
    .read = proc_audio_sample_rate_read,
};

static const struct file_operations proc_video_edid_fops = {
    .owner = THIS_MODULE,
    .read = proc_video_edid_read,
    .write = proc_video_edid_write,
};

static const struct file_operations proc_video_edid_snapshot_fops = {
    .owner = THIS_MODULE,
    .read = proc_video_edid_snapshot_read,
};

static const struct file_operations proc_version_fops = {
    .owner = THIS_MODULE,
    .read = proc_version_read,
    .write = proc_version_write,
};

void proc_buffer_init(void)
{
    // video_status_buffer = "disappear"
    snprintf(video_status_buffer, sizeof(video_status_buffer), "disappear\n");
    video_status_buffer_length = strlen(video_status_buffer);
    // video_width_buffer = "0"
    snprintf(video_width_buffer, sizeof(video_width_buffer), "0\n");
    video_width_buffer_length = strlen(video_width_buffer);
    // vidio_height_buffer = "0"
    snprintf(vidio_height_buffer, sizeof(vidio_height_buffer), "0\n");
    video_height_buffer_length = strlen(vidio_height_buffer);
    // video_fps_buffer = "0"
    snprintf(video_fps_buffer, sizeof(video_fps_buffer), "0\n");
    video_fps_buffer_length = strlen(video_fps_buffer);
    // video_hdcp_buffer = "0"
    snprintf(video_hdcp_buffer, sizeof(video_hdcp_buffer), "no hdcp\n");
    video_hdcp_buffer_length = strlen(video_hdcp_buffer);
    // audio_sample_rate_buffer = "0"
    snprintf(audio_sample_rate_buffer, sizeof(audio_sample_rate_buffer), "disappear\n");
    audio_sample_rate_buffer_length = strlen(audio_sample_rate_buffer);
}

int proc_info_init(void)
{
    // create proc dir
    proc_lt7911_dir = proc_mkdir(PROC_LT7911_DIR, NULL);
    if (!proc_lt7911_dir) {
        goto err;
    }

    // create proc video pwr ctrl files
    proc_video_power_file = proc_create(PROC_VIDEO_PWR, 0666, proc_lt7911_dir, &proc_video_power_fops);    // W&R
    if (!proc_video_power_file) {
        goto err;
    }

    // create proc hdmi info files
    proc_video_status_file = proc_create(PROC_VIDEO_STATUS, 0666, proc_lt7911_dir, &proc_video_status_fops); // W&R
    if (!proc_video_status_file) {
        goto err;
    }

    // create proc hdmi width and height files
    proc_video_width_file = proc_create(PROC_VIDEO_WIDTH, 0444, proc_lt7911_dir, &proc_video_width_fops); // R
    if (!proc_video_width_file) {
        goto err;
    }

    // create proc hdmi height file
    proc_video_height_file = proc_create(PROC_VIDEO_HEIGHT, 0444, proc_lt7911_dir, &proc_video_height_fops); // R
    if (!proc_video_height_file) {
        goto err;
    }

    // create proc hdmi fps file
    proc_video_fps_file = proc_create(PROC_VIDEO_FPS, 0444, proc_lt7911_dir, &proc_video_fps_fops); // R
    if (!proc_video_fps_file) {
        goto err;
    }

    // create proc hdmi hdcp file
    proc_video_hdcp_file = proc_create(PROC_VIDEO_HDCP, 0444, proc_lt7911_dir, &proc_video_hdcp_fops); // R
    if (!proc_video_hdcp_file) {
        goto err;
    }

    // create proc audio sample rate file
    proc_audio_sample_rate_file = proc_create(PROC_AUDIO_SAMPLE_RATE, 0444, proc_lt7911_dir, &proc_audio_sample_rate_fops); // R
    if (!proc_audio_sample_rate_file) {
        goto err;
    }

    // create proc hdmi edid file
    proc_video_edid_file = proc_create(PROC_VIDEO_EDID, 0666, proc_lt7911_dir, &proc_video_edid_fops); // W&R
    if (!proc_video_edid_file) {
        goto err;
    }

    // create proc hdmi edid snapshot file
    proc_video_edid_snapshot_file = proc_create(PROC_VIDEO_EDID_SNAPSHOT, 0444, proc_lt7911_dir, &proc_video_edid_snapshot_fops); // R
    if (!proc_video_edid_snapshot_file) {
        goto err;
    }

    // create proc version file
    proc_version_file = proc_create(PROC_VERSION, 0666, proc_lt7911_dir, &proc_version_fops); // W&R
    if (!proc_version_file) {
        goto err;
    }

    return 0;

err:
    printk(KERN_ERR "Failed to create proc entries\n");
    if (proc_audio_sample_rate_file) remove_proc_entry(PROC_AUDIO_SAMPLE_RATE, proc_lt7911_dir);
    if (proc_video_hdcp_file) remove_proc_entry(PROC_VIDEO_HDCP, proc_lt7911_dir);
    if (proc_video_fps_file) remove_proc_entry(PROC_VIDEO_FPS, proc_lt7911_dir);
    if (proc_video_width_file) remove_proc_entry(PROC_VIDEO_WIDTH, proc_lt7911_dir);
    if (proc_video_height_file) remove_proc_entry(PROC_VIDEO_HEIGHT, proc_lt7911_dir);
    if (proc_video_status_file) remove_proc_entry(PROC_VIDEO_STATUS, proc_lt7911_dir);
    if (proc_video_power_file) remove_proc_entry(PROC_VIDEO_PWR, proc_lt7911_dir);
    if (proc_video_edid_file) remove_proc_entry(PROC_VIDEO_EDID, proc_lt7911_dir);
    if (proc_video_edid_snapshot_file) remove_proc_entry(PROC_VIDEO_EDID_SNAPSHOT, proc_lt7911_dir);
    if (proc_version_file) remove_proc_entry(PROC_VERSION, proc_lt7911_dir);

    if (proc_lt7911_dir) remove_proc_entry(PROC_LT7911_DIR, NULL);
    return -1;
}

int proc_info_exit(void)
{
    if (proc_audio_sample_rate_file) remove_proc_entry(PROC_AUDIO_SAMPLE_RATE, proc_lt7911_dir);
    if (proc_video_hdcp_file) remove_proc_entry(PROC_VIDEO_HDCP, proc_lt7911_dir);
    if (proc_video_fps_file) remove_proc_entry(PROC_VIDEO_FPS, proc_lt7911_dir);
    if (proc_video_width_file) remove_proc_entry(PROC_VIDEO_WIDTH, proc_lt7911_dir);
    if (proc_video_height_file) remove_proc_entry(PROC_VIDEO_HEIGHT, proc_lt7911_dir);
    if (proc_video_status_file) remove_proc_entry(PROC_VIDEO_STATUS, proc_lt7911_dir);
    if (proc_video_power_file) remove_proc_entry(PROC_VIDEO_PWR, proc_lt7911_dir);
    if (proc_video_edid_file) remove_proc_entry(PROC_VIDEO_EDID, proc_lt7911_dir);
    if (proc_video_edid_snapshot_file) remove_proc_entry(PROC_VIDEO_EDID_SNAPSHOT, proc_lt7911_dir);
    if (proc_version_file) remove_proc_entry(PROC_VERSION, proc_lt7911_dir);

    if (proc_lt7911_dir) remove_proc_entry(PROC_LT7911_DIR, NULL);
    return 0;
}

int lt7911_pwr_ctrl(int pwr_en)
{
    int ret;

    if (pwr_en) {
        // Power on the LT7911
        ret = gpio_direction_output(PWR_PIN, 1);
        if (ret < 0) {
            printk(KERN_ERR "Failed to set GPIO %d to high\n", PWR_PIN);
            return -1;
        }
        // update the proc file
        snprintf(video_power_buffer, sizeof(video_power_buffer), "on\n");
        video_power_buffer_length = strlen(video_power_buffer);
    } else {
        // Power off the LT7911
        ret = gpio_direction_output(PWR_PIN, 0);
        if (ret < 0) {
            printk(KERN_ERR "Failed to set GPIO %d to low\n", PWR_PIN);
            return -1;
        }
        // update the proc file
        snprintf(video_power_buffer, sizeof(video_power_buffer), "off\n");
        video_power_buffer_length = strlen(video_power_buffer);
    }

    return 0;
}

int gpio_init(void)
{
    int ret;

    // Initialize GPIO for LT7911 INT pin
    // init GPIO Interrupt
    if (!gpio_is_valid(INT_PIN)) {
        printk(KERN_ERR "Invalid GPIO pin\n");
        return -1;
    }

    if (gpio_request(INT_PIN, "init_gpio") < 0) {
        printk(KERN_ERR "GPIO request failed\n");
        return -1;
    }

    if ((irq_number = gpio_to_irq(INT_PIN)) < 0) {
        printk(KERN_ERR "Unable to get IRQ number\n");
        gpio_free(INT_PIN);
        return -1;
    }

    ret = request_irq(irq_number, gpio_irq_handler,
                     Int_Action, "init_gpio_irq", NULL);
    if (ret) {
        printk(KERN_ERR "Unable to request IRQ\n");
        gpio_free(INT_PIN);
        return ret;
    }

    // Initialize GPIO for LT7911 PWR pin
    if (PWR_PIN >= 0) {
        ret = gpio_request(PWR_PIN, "LT7911_PWR");
        if (ret) {
            printk(KERN_ERR "Failed to request GPIO %d for LT7911 PWR pin\n", PWR_PIN);
            free_irq(irq_number, NULL);
            gpio_free(INT_PIN);
            return ret;
        }
    }

    // Setup for version detect
    lt7911_pwr_ctrl(1); // Power on the LT7911

    return 0;
}

int gpio_exit(void)
{
    // Free the IRQ and GPIO resources
    free_irq(irq_number, NULL);

    if (INT_PIN >= 0) {
        gpio_free(INT_PIN);
    }
    if (PWR_PIN >= 0) {
        gpio_free(PWR_PIN);
    }

    return 0;
}

/* return 0 : normal res;
 * return 1 : new res;
 * return 2 : unsupport res;
 * return 3 : unknow res;
 */
u8 check_res(u16 _width, u16 _height)
{
    u8 i;
	static u16 old_width = 0xFFFF;
	static u16 old_height = 0xFFFF;

    for(i = 0; i < sizeof(hdmi_res_list)/4; i++){
        // printk(KERN_INFO "hdmi_res_list[%d] = %dx%d\n", i, hdmi_res_list[i][0], hdmi_res_list[i][1]);
        if (_width == hdmi_res_list[i][0] && _height == hdmi_res_list[i][1]) {
            if (old_width != _width || old_height != _height) {
                old_width = _width;
                old_height = _height;
                return NEW_RES;
            }
            return NORMAL_RES;
        }
    }
    for(i = 0; i < sizeof(hdmi_unsupported_res_list)/4; i++){
        if (_width == hdmi_unsupported_res_list[i][0] && _height == hdmi_unsupported_res_list[i][1]) return UNSUPPORT_RES;
    }
    return UNKNOWN_RES;
}

// check if the I2C device exists
bool i2c_device_exists(u8 device_address) {
    struct i2c_client *f_client;
    struct i2c_adapter *f_adapter;
    bool exists = false;
    int ret;

    // 获取 I2C 适配器
    f_adapter = i2c_get_adapter(i2c_bus_num);
    if (!f_adapter) {
        printk("Failed to get I2C adapter\n");
        return false;
    }

    // 创建 I2C 客户端
    f_client = i2c_new_dummy(f_adapter, device_address);
    if (!f_client) {
        printk(KERN_ERR "Failed to create I2C device\n");
        return -ENODEV;
    }

    // 尝试读取设备
    ret = i2c_smbus_read_byte(f_client);
    exists = (ret >= 0);  // 如果返回值非负，设备存在

    // 释放资源
    i2c_put_adapter(f_adapter);
    i2c_unregister_device(f_client);

    return exists;
}

static int lt7911_dt_parse(struct i2c_client *i2c)
{
    struct device_node *np;
    u32 reg = 0;
    int irq_gpio;
    int reset_gpio;

    if (!i2c) {
        printk(KERN_ERR "lt7911: invalid i2c client\n");
        return -EINVAL;
    }

    np = i2c->dev.of_node;
    if (!np) {
        printk(KERN_ERR "lt7911: dts node not found\n");
        return -ENODEV;
    }

    irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
    reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
    if (!gpio_is_valid(irq_gpio) || !gpio_is_valid(reset_gpio)) {
        printk(KERN_ERR "lt7911: invalid irq-gpio/reset-gpio in dts\n");
        of_node_put(np);
        return -EINVAL;
    }

    if (of_property_read_u32(np, "reg", &reg)) {
        printk(KERN_ERR "lt7911: missing reg in dts\n");
        of_node_put(np);
        return -EINVAL;
    }
    lt7911_i2c_addr = (u8)(reg & 0x7F);

    i2c_bus_num = i2c->adapter ? i2c->adapter->nr : -1;

    INT_PIN = irq_gpio;
    PWR_PIN = reset_gpio;
    dt_cfg_valid = true;

    printk(KERN_INFO "lt7911: config bus=%d addr=0x%02x irq_gpio=%d reset_gpio=%d\n",
           i2c_bus_num, lt7911_i2c_addr, INT_PIN, PWR_PIN);

    return 0;
}

// I2C write function
int i2c_write_byte(u8 offset, u8 reg, u8 data)
{
    int ret;

    // if offset is changed, write it first
    if (offset != old_offset) {
        ret = i2c_smbus_write_byte_data(client, LT7911_REG_OFFSET, offset);
        if (ret < 0) {
            printk(KERN_ERR "[W]Failed to write offset to the i2c bus; ret = %d\n", ret);
            return -1;
        }
        old_offset = offset;
    }

    // write the data to the i2c bus
    ret = i2c_smbus_write_byte_data(client, reg, data);
    if (ret < 0) {
        printk(KERN_ERR "[W]Failed to write data to the i2c bus; ret = %d\n", ret);
        return -1;
    }

    return 0;
}

// I2C read function
int i2c_read_byte(u8 offset, u8 reg, u8 *data)
{
    int ret;

    // if offset is changed, write it first
    if (offset != old_offset) {
        ret = i2c_smbus_write_byte_data(client, LT7911_REG_OFFSET, offset);
        if (ret < 0) {
            printk(KERN_ERR "[W]Failed to write offset to the i2c bus; ret = %d\n", ret);
            return -1;
        }
        old_offset = offset;
    }

    ret = i2c_smbus_read_byte_data(client, reg);
    if (ret < 0) {
        printk(KERN_ERR "[R]Failed to read data from the i2c bus; ret = %d\n", ret);
        return -1;
    }

    // ret > 0
    *data = (u8)ret;

    return 0;
}

// #define i2c_wr_single_func
#ifdef i2c_wr_single_func
// I2C write function
int i2c_write_bytes(u8 offset, u8 reg, u8 *data, u16 len)
{
    u8 buf[2] = {0};
    int ret;
    u16 count;

    // if offset is changed, write it first
    if (offset != old_offset) {
        ret = i2c_smbus_write_byte_data(client, LT7911_REG_OFFSET, offset);
        if (ret < 0) {
            printk(KERN_ERR "[W]Failed to write offset to the i2c bus; ret = %d\n", ret);
            return -1;
        }
        old_offset = offset;
    }

    // write the data to the i2c bus
    for (count = 0; count < len; count ++){
        ret = i2c_smbus_write_byte_data(client, reg, *(data+count));
        if (ret < 0) {
            printk(KERN_ERR "[W]Failed to write data to the i2c bus; ret = %d\n", ret);
            return -1;
        }
    }

    return 0;
}

// I2C read function
int i2c_read_bytes(u8 offset, u8 reg, u8 *data, u16 len)
{
    int ret;
    u16 count;

    // if offset is changed, write it first
    if (offset != old_offset) {
        ret = i2c_smbus_write_byte_data(client, LT7911_REG_OFFSET, offset);
        if (ret < 0) {
            printk(KERN_ERR "[W]Failed to write offset to the i2c bus; ret = %d\n", ret);
            return -1;
        }
        old_offset = offset;
    }

    // read the data from the i2c bus
    for (count = 0; count < len; count ++) {
        ret = i2c_smbus_read_byte_data(client, reg);
        if (ret < 0) {
            printk(KERN_ERR "[R]Failed to read data from the i2c bus; ret = %d\n", ret);
            return -1;
        }   // ret > 0
        *(data+count) = (u8)ret;
    }

    return 0;
}
#else

// I2C write function
int i2c_write_bytes(u8 offset, u8 reg, u8 *data, u8 len)
{
    int ret;

    // if offset is changed, write it first
    if (offset != old_offset) {
        ret = i2c_smbus_write_byte_data(client, LT7911_REG_OFFSET, offset);
        if (ret < 0) {
            printk(KERN_ERR "[W]Failed to write offset to the i2c bus; ret = %d\n", ret);
            return -1;
        }
        old_offset = offset;
    }

    // write the data to the i2c bus
    ret = i2c_smbus_write_i2c_block_data(client, reg, len, data);
    if (ret < 0) {
        printk(KERN_ERR "[W]Failed to write data to the i2c bus; ret = %d\n", ret);
        return -1;
    }

    return 0;
}

// I2C read function
int i2c_read_bytes(u8 offset, u8 reg, u8 *data, u8 len)
{
    int ret;

    // if offset is changed, write it first
    if (offset != old_offset) {
        ret = i2c_smbus_write_byte_data(client, LT7911_REG_OFFSET, offset);
        if (ret < 0) {
            printk(KERN_ERR "[W]Failed to write offset to the i2c bus; ret = %d\n", ret);
            return -1;
        }
        old_offset = offset;
    }

    ret = i2c_smbus_read_i2c_block_data(client, reg, len, data);
    if (ret < 0) {
        printk(KERN_ERR "[R]Failed to read data from the i2c bus; ret = %d\n", ret);
        return -1;
    }


    return 0;
}
#endif

int lt7911_enable(void) {
    // Enable the LT7911 by writing to the appropriate register
    if (i2c_write_byte(LT7911_SYS_OFFSET, 0xEE, 0x01) != 0) {
        printk(KERN_ERR "Failed to enable LT7911D\n");
        return -1;
    }
    return 0;
}

int lt7911_disable(void) {
    // Disable the LT7911D by writing to the appropriate register
    if (i2c_write_byte(LT7911_SYS_OFFSET, 0xEE, 0x00) != 0) {
        printk(KERN_ERR "Failed to disable LT7911D\n");
        return -1;
    }
    return 0;
}

int lt7911_disable_watchdog(void) {
    // Disable the LT7911 watchdog timer
    if (chip_platform == LT7911_CHIP_LT7911D) {
        // LT7911D
        printk(KERN_ERR "LT7911D chip platform unsupported\n");
        // can't be returned
        // return -1;
    } else {
        printk(KERN_ERR "Unknown chip platform\n");
        return -1;
    }
    return 0;
}

// function for checking the chip i2c register
int check_chip_register(void)
{
    u8 chip_id[4] = {0};

    lt7911_enable();

    // Read the chip ID register LT7911D
    if (i2c_read_byte(LT7911_SYS4_OFFSET, 0x00, chip_id) < 0) return -1;
    if (i2c_read_byte(LT7911_SYS4_OFFSET, 0x01, chip_id + 1) < 0) return -1;

    if (chip_id[0] == 0x16 && chip_id[1] == 0x05) {
        chip_platform = LT7911_CHIP_LT7911D;
        printk(KERN_INFO "Chip: LT7911D\n");
        return 1; // LT7911D
    }

    chip_platform = LT7911_CHIP_UNKNOWN;
    return -1;
}

int lt7911_get_signal_state(u8 *p_state)
{
    u8 state = 0;
    u8 vactive[2];
    u8 hactive[2];
    u16 vactive_val;
    u16 hactive_val;
    int ret = 0;

    if (chip_platform == LT7911_CHIP_LT7911D) {
        // LT7911D
        // start to measure
        if (i2c_write_byte(LT7911D_HDMI_INFO_OFFSET, 0x83, 0x10) != 0) return -1;
        // delay 5ms
        msleep(5);
        // read HDMI signal
        if (i2c_read_byte(LT7911D_HDMI_INFO_OFFSET, 0x96, vactive  ) != 0) return -1;
        if (i2c_read_byte(LT7911D_HDMI_INFO_OFFSET, 0x97, vactive+1) != 0) return -1;
        if (i2c_read_byte(LT7911D_HDMI_INFO_OFFSET, 0x8B, hactive  ) != 0) return -1;
        if (i2c_read_byte(LT7911D_HDMI_INFO_OFFSET, 0x8C, hactive+1) != 0) return -1;

        vactive_val = (vactive[0] << 8) | vactive[1];
        hactive_val = (hactive[0] << 8) | hactive[1];

        // check HDMI signal
        if (vactive_val == 0 || hactive_val == 0) {
            state &= ~0x01; // HDMI signal disappear
        } else {
            state |= 0x01;  // HDMI signal stable
        }
        // LT7911D Audio default is stable
        state |= 0x02;
    } else {
        printk(KERN_ERR "Unknown chip platform\n");
        return -1;
    }

    *p_state = state;
    return ret;
}

int lt7911_get_csi_res(u16 *p_width, u16 *p_height)
{
    u8 val[4];
    u8 res_type;
	u16 height;
	u16 width;

    // Getting Hactive & Vactive
    if (chip_platform == LT7911_CHIP_LT7911D) {
        // LT7911D
        if (i2c_read_byte(LT7911D_CSI_INFO_OFFSET, 0x06, val  ) != 0) return -1;
        if (i2c_read_byte(LT7911D_CSI_INFO_OFFSET, 0x07, val+1) != 0) return -1;
        // width
        if (i2c_read_byte(LT7911D_CSI_INFO_OFFSET, 0x38, val+2) != 0) return -1;
        if (i2c_read_byte(LT7911D_CSI_INFO_OFFSET, 0x39, val+3) != 0) return -1;
    } else {
        printk(KERN_ERR "Unknown chip platform\n");
        return -1;
    }

    height = (val[0] << 8) | val[1];
    width = (val[2] << 8) | val[3];

    *p_width = width;
    *p_height = height;

    res_type = check_res(width, height);

    // switch (res_type)
    // {
    // case NORMAL_RES:
    //     printk("[hdmi] get res : %d * %d\n", width, height);
    //     break;
    // case NEW_RES:
    //     printk("[hdmi] get new res : %d * %d\n", width, height);
    //     // write_res_to_file(width, height);
    //     break;
    // case UNSUPPORT_RES:
    //     printk("[hdmi] get unsupport res : %d * %d\n", width, height);
    //     break;
    // case UNKNOWN_RES:
    //     printk("[hdmi] get unknown res : %d * %d\n", width, height);
    //     break;
    // }

	return res_type;
}

int lt7911_get_csi_fps(u16 *p_fps, u16 width, u16 height)
{
    u8 val[4];
    u16 HTotal, VTotal;
    u32 clk = 0;
    u32 fps = 0;

    if (chip_platform == LT7911_CHIP_LT7911D) {
        // DP video source select from DP RX
        if (i2c_write_byte(LT7911D_HDMI_INFO_OFFSET, 0x83, 0x10) != 0) return -1;

        // horizontal timing values are half of the real timing and must be x2.
        if (i2c_read_byte(LT7911D_HDMI_INFO_OFFSET, 0x89, val) != 0) return -1;
        if (i2c_read_byte(LT7911D_HDMI_INFO_OFFSET, 0x8A, val+1) != 0) return -1;
        if (i2c_read_byte(LT7911D_HDMI_INFO_OFFSET, 0x9E, val+2) != 0) return -1;
        if (i2c_read_byte(LT7911D_HDMI_INFO_OFFSET, 0x9F, val+3) != 0) return -1;

        HTotal = (val[0] << 8) | val[1];
        VTotal = (val[2] << 8) | val[3];
        HTotal *= 2;

        printk(KERN_INFO "HDMI HTotal: %d, VTotal: %d\n", HTotal, VTotal);
        msleep(20);

        if (i2c_write_byte(LT7911_SYS4_OFFSET, 0x34, 0x21) != 0) return -1;
        msleep(10);

        if (i2c_read_byte(0xB8, 0xB1, val) != 0) return -1;
        if (i2c_read_byte(0xB8, 0xB2, val+1) != 0) return -1;
        if (i2c_read_byte(0xB8, 0xB3, val+2) != 0) return -1;

        clk = ((val[0] & 0x07) << 16) | (val[1] << 8) | val[2];
        fps = (clk * 2000) / (HTotal * VTotal);
    } else {
        printk(KERN_ERR "Unknown chip platform\n");
        return -1;
    }

    *p_fps = (u16)fps;

    return 0;
}

int lt7911_get_hdcp_mode(u8 *p_hdcp)
{
    u8 val = 0;

    if (chip_platform == LT7911_CHIP_LT7911D) {
        // LT7911D not support HDCP
        printk(KERN_ERR "LT7911D chip platform does not support HDCP\n");
        val = 0;
        // return 0;
    } else {
        printk(KERN_ERR "Unknown chip platform\n");
        return -1;
    }
    *p_hdcp = val;
    /*
    HDCP mode:
    0x00: No HDCP
    0x01: HDCP 1.4
    0x02: HDCP 2.2
    */
    return 0;
}

int lt7911_get_audio_sample_rate(u8 *p_sample_rate)
{
    u8 val;
    u8 val_buf[5];
    if (chip_platform == LT7911_CHIP_LT7911D) {
        // LT7911D
        // Read audio sample rate
        if (i2c_read_byte(LT7911D_AUDIO_INFO_OFFSET, 0x55, &val) != 0) return -1;
        // return 0;
    } else {
        printk(KERN_ERR "Unknown chip platform\n");
        return -1;
    }

    *p_sample_rate = val;
    return 0;
}

bool compare_arrays(const unsigned char arr1[], const unsigned char arr2[], size_t length)
{
    u8 i;
    for (i = 0; i < length; i++) {
        if (arr1[i] != arr2[i]) {
            return false; // 一旦发现不一致，返回 false
        }
    }
    return true; // 全部元素一致，返回 true
}

int check_edid(u8 *edid_data, u16 edid_size)
{
    u16 i;
    u8 checksum1 = 0;
    u8 checksum2 = 0;

    // Check if the EDID data length is valid
    if (edid_size != EDID_BUFFER_SIZE) {
        printk(KERN_ERR "EDID data length is not %d bytes\n", EDID_BUFFER_SIZE);
        return -1; // EDID data length is not enough
    }

    // Check EDID header
    if (edid_data[0] != 0x00 || edid_data[1] != 0xFF ||
        edid_data[2] != 0xFF || edid_data[3] != 0xFF ||
        edid_data[4] != 0xFF || edid_data[5] != 0xFF ||
        edid_data[6] != 0xFF || edid_data[7] != 0x00) {
        printk(KERN_ERR "EDID header is invalid\n");
        return -1; // EDID header is invalid
    }

    // First 128 Bytes checksum
    for (i = 0; i < 127; i++) {
        checksum1 += edid_data[i];
    }
    checksum1 = 0x100 - checksum1; // Reverse
    if (checksum1 != edid_data[127]) {
        // Checksum for first 128 bytes is incorrect
        printk(KERN_ERR "Checksum for first 128 bytes is incorrect\n");
        return -1;
    }

    // Second 128 Bytes checksum
    for (i = 128; i < 255; i++) {
        checksum2 += edid_data[i];
    }
    checksum2 = 0x100 - checksum2; // Reverse
    if (checksum2 != edid_data[255]) {
        // Checksum for second 128 bytes is incorrect
        printk(KERN_ERR "Checksum for second 128 bytes is incorrect\n");
        return -1;
    }

    return 0; // EDID is valid
}

int lt7911_edid_write(u8 *edid_data, u16 edid_size)
{
    u8 i;
    u8 chip_data[16] = {0};
    u8 wr_count = edid_size / LT7911D_WR_SIZE + 1;
    u8 version_str[32] = {0};

#ifndef RE_WRITE_VERSION
    // read version from chip
    if (lt7911_str_read(version_str) < 0) {
        return -EIO; // version read failed
    }
#endif
    // check chip platform
    if (chip_platform == LT7911_CHIP_LT7911D) {
        // printk(KERN_INFO "Writing EDID...\n");
        printk(KERN_ERR "LT7911D chip platform does not support EDID writing\n");
        // printk(KERN_INFO "EDID write completed\n");
    } else {
        printk(KERN_ERR "Not currently supporting EDID writing outside of LT7911\n");
    }

    return 0;
}

int lt7911_edid_read(u8 *edid_data, u16 edid_size)
{
    u8 i;
    u8 wr_count = edid_size / LT7911D_WR_SIZE;

    // Read EDID data from LT7911D
    if (chip_platform == LT7911_CHIP_LT7911D) {
        // printk(KERN_INFO "Reading EDID...\n");
        printk(KERN_ERR "LT7911D chip platform does not support EDID reading\n");
        return -1;
    } else {
        printk(KERN_ERR "Not currently supporting EDID reading outside of LT7911\n");
        return -1;
    }
    return 0;
}

int lt7911_str_write(u8 *str, u16 len)
{
    u8 i;
    u8 chip_data[16] = {0};
    u8 version_str[LT7911D_WR_SIZE] = {0};

    // read version from chip
    if (lt7911_str_read(version_str) < 0) {
        return -1; // version read failed
    }

    for (i = 0; i < LT7911D_WR_SIZE; i++) {
        // printk(KERN_INFO "version_str[%d] = 0x%02x\n", i, version_str[i]);
        if (version_str[i] != 0xFF && version_str[i] != 0x00) {
            return -1; // version string is not empty
        }
    }

    // max len = LT7911D_WR_SIZE;
    if (len > LT7911D_WR_SIZE) {
        printk(KERN_ERR "String length exceeds maximum of %d bytes\n", LT7911D_WR_SIZE);
        return -1;
    }

    // check chip platform
    if (chip_platform == LT7911_CHIP_LT7911D) {
        // printk(KERN_INFO "Writing String...\n");
        printk(KERN_ERR "LT7911D chip platform does not support String writing\n");
        // printk(KERN_INFO "EDID write completed\n");
    } else {
        printk(KERN_ERR "Not currently supporting String writing outside of LT7911\n");
    }

    return 0;
}

int lt7911_str_read(u8 *str)
{
    int i;
    // Read String data from LT7911D
    if (chip_platform == LT7911_CHIP_LT7911D) {
        // printk(KERN_INFO "Reading String...\n");
        printk(KERN_ERR "LT7911D chip platform does not support String reading\n");
        return -1;
    } else {
        printk(KERN_ERR "Not currently supporting String reading outside of LT7911\n");
        return -1;
    }
    for (i = 0; i < LT7911D_WR_SIZE; i++) {
        if (str[i] == 0xFF) {
            str[i] = '\0'; // Replace null byte with string terminator
        }
    }
    return 0;
}

void hdmi_change_process(u8 hdmi_state)
{
    int hdmi_type;
	u16 height = 0;
	u16 width = 0;
	// u16 height = 2560;
	// u16 width = 1080;
    u16 fps = 0;
    u8 hdcp_mode;
    if (hdmi_state == 1) {
        // HDMI signal is stable
        printk(KERN_INFO "HDMI signal is stable\n");
        hdmi_type = lt7911_get_csi_res(&width, &height);
        printk(KERN_INFO "Detected HDMI resolution type: %d\n", hdmi_type);
        if (hdmi_type < 0) {
            printk(KERN_ERR "Failed to get HDMI resolution\n");
            lt7911_disable();
            return;
        }
        // Update proc file content
        switch (hdmi_type)
        {
        case NEW_RES:
            // snprintf(video_status_buffer, sizeof(video_status_buffer), "new res\n");
            // video_status_buffer_length = strlen(video_status_buffer);
            // break;
        case NORMAL_RES:
            // snprintf(video_status_buffer, sizeof(video_status_buffer), "normal res\n");
            // snprintf(video_status_buffer, sizeof(video_status_buffer), "new res\n");
            // video_status_buffer_length = strlen(video_status_buffer);
            // break;
        case UNSUPPORT_RES:
            // snprintf(video_status_buffer, sizeof(video_status_buffer), "unsupport res\n");
            // snprintf(video_status_buffer, sizeof(video_status_buffer), "new res\n");
            // video_status_buffer_length = strlen(video_status_buffer);
            // break;
        case UNKNOWN_RES:
            // snprintf(video_status_buffer, sizeof(video_status_buffer), "unknown res\n");
            snprintf(video_status_buffer, sizeof(video_status_buffer), "new res\n");
            video_status_buffer_length = strlen(video_status_buffer);
            break;
        case ERROR_RES:
            snprintf(video_status_buffer, sizeof(video_status_buffer), "error res\n");
            snprintf(video_width_buffer, sizeof(video_width_buffer), "%d\n", 0);
            snprintf(vidio_height_buffer, sizeof(vidio_height_buffer), "%d\n", 0);
            video_status_buffer_length = strlen(video_status_buffer);
            video_width_buffer_length = strlen(video_width_buffer);
            video_height_buffer_length = strlen(vidio_height_buffer);
            printk(KERN_ERR "Error resolution detected\n");
            lt7911_disable();
            return;
        default:
            break;
        }
        snprintf(video_width_buffer, sizeof(video_width_buffer), "%d\n", width);
        snprintf(vidio_height_buffer, sizeof(vidio_height_buffer), "%d\n", height);
        video_width_buffer_length = strlen(video_width_buffer);
        video_height_buffer_length = strlen(vidio_height_buffer);
        printk(KERN_INFO "HDMI resolution: %d x %d\n", width, height);

        // Get HDCP mode
        lt7911_get_hdcp_mode(&hdcp_mode);
        switch (hdcp_mode)
        {
        case 0x00:
            // printk(KERN_INFO "HDCP mode: No HDCP\n");
            snprintf(video_hdcp_buffer, sizeof(video_hdcp_buffer), "no hdcp\n");
            break;
        case 0x01:
            // printk(KERN_INFO "HDCP mode: HDCP 1.4\n");
            snprintf(video_hdcp_buffer, sizeof(video_hdcp_buffer), "hdcp 1.4\n");
            break;
        case 0x02:
            // printk(KERN_INFO "HDCP mode: HDCP 2.2\n");
            snprintf(video_hdcp_buffer, sizeof(video_hdcp_buffer), "hdcp 2.2\n");
            break;

        default:
            // printk(KERN_ERR "Unknown HDCP mode: %02x\n", hdcp_mode);
            snprintf(video_hdcp_buffer, sizeof(video_hdcp_buffer), "unknown hdcp\n");
            break;
        }
        video_hdcp_buffer_length = strlen(video_hdcp_buffer);
        printk(KERN_INFO "HDCP mode: %s\n", video_hdcp_buffer);

        // Get CSI fps
        lt7911_get_csi_fps(&fps, width, height);
        snprintf(video_fps_buffer, sizeof(video_fps_buffer), "%d\n", fps);
        video_fps_buffer_length = strlen(video_fps_buffer);
        printk(KERN_INFO "HDMI FPS: %d\n", fps);

        atomic_inc(&lt7911_ctx.status);
        wake_up_interruptible(&lt7911_ctx.wait_queue);

    } else if (hdmi_state == 0) {
        // HDMI signal has disappeared
        snprintf(video_status_buffer, sizeof(video_status_buffer), "disappear\n");
        snprintf(video_width_buffer, sizeof(video_width_buffer), "%d\n", 0);
        snprintf(vidio_height_buffer, sizeof(vidio_height_buffer), "%d\n", 0);
        snprintf(video_fps_buffer, sizeof(video_fps_buffer), "%d\n", 0);
        snprintf(video_hdcp_buffer, sizeof(video_hdcp_buffer), "no hdcp\n");
        video_status_buffer_length = strlen(video_status_buffer);
        video_width_buffer_length = strlen(video_width_buffer);
        video_height_buffer_length = strlen(vidio_height_buffer);
        video_fps_buffer_length = strlen(video_fps_buffer);
        video_hdcp_buffer_length = strlen(video_hdcp_buffer);
        printk(KERN_INFO "HDMI signal has disappeared\n");

        atomic_inc(&lt7911_ctx.status);
        wake_up_interruptible(&lt7911_ctx.wait_queue);

        lt7911_disable();
        return;
    } else {
        // unknown HDMI state
        printk(KERN_ERR "Unknown HDMI state: %d\n", hdmi_state);
        snprintf(video_status_buffer, sizeof(video_status_buffer), "unknown\n");
        snprintf(video_width_buffer, sizeof(video_width_buffer), "%d\n", 0);
        snprintf(vidio_height_buffer, sizeof(vidio_height_buffer), "%d\n", 0);
        snprintf(video_fps_buffer, sizeof(video_fps_buffer), "%d\n", 0);
        snprintf(video_hdcp_buffer, sizeof(video_hdcp_buffer), "unknown hdcp\n");
        video_status_buffer_length = strlen(video_status_buffer);
        video_width_buffer_length = strlen(video_width_buffer);
        video_height_buffer_length = strlen(vidio_height_buffer);
        video_fps_buffer_length = strlen(video_fps_buffer);
        video_hdcp_buffer_length = strlen(video_hdcp_buffer);

        atomic_inc(&lt7911_ctx.status);
        wake_up_interruptible(&lt7911_ctx.wait_queue);

        lt7911_disable();
        return;
    }
}

void lt7911_force_resolution(u16 width, u16 height, int fps)
{
    snprintf(video_status_buffer, sizeof(video_status_buffer), "new res\n");
    snprintf(video_width_buffer, sizeof(video_width_buffer), "%d\n", width);
    snprintf(vidio_height_buffer, sizeof(vidio_height_buffer), "%d\n", height);
    video_status_buffer_length = strlen(video_status_buffer);
    video_width_buffer_length = strlen(video_width_buffer);
    video_height_buffer_length = strlen(vidio_height_buffer);
    if (fps > 0) {
        snprintf(video_fps_buffer, sizeof(video_fps_buffer), "%d\n", fps);
        video_fps_buffer_length = strlen(video_fps_buffer);
    } else {
        snprintf(video_fps_buffer, sizeof(video_fps_buffer), "%d\n", 30);
        video_fps_buffer_length = strlen(video_fps_buffer);
    }

    atomic_inc(&lt7911_ctx.status);
    wake_up_interruptible(&lt7911_ctx.wait_queue);
}

void audio_change_process(u8 audio_state)
{
    u8 sample_rate;
    int ret;
    // Handle audio change events
    switch (audio_state)
    {
    case 0:
        // Audio signal is disappearing
        snprintf(audio_sample_rate_buffer, sizeof(audio_sample_rate_buffer), "disappear\n");
        audio_sample_rate_buffer_length = strlen(audio_sample_rate_buffer);
        // printk(KERN_INFO "Audio signal has disappeared\n");
        break;
    case 1:
        // Audio signal is stable
        ret = lt7911_get_audio_sample_rate(&sample_rate);
        if (ret < 0) {
            printk(KERN_ERR "Failed to get audio sample rate\n");
            snprintf(audio_sample_rate_buffer, sizeof(audio_sample_rate_buffer), "unknown\n");
            audio_sample_rate_buffer_length = strlen(audio_sample_rate_buffer);
            return;
        }
        snprintf(audio_sample_rate_buffer, sizeof(audio_sample_rate_buffer), "%d\n", sample_rate);
        audio_sample_rate_buffer_length = strlen(audio_sample_rate_buffer);
        // printk(KERN_INFO "Audio signal is stable\n");
        break;
    default:
        break;
    }
}

static void get_hdmi_info_handler(struct work_struct *work)
{
    u8 signal_state;
    int ret;

    if (!lt7911_init_done)
        return;

    if (lt7911_enable() < 0) return;
    if (lt7911_disable_watchdog() < 0) {
        printk(KERN_ERR "Failed to disable LT7911D watchdog\n");
        lt7911_disable();
        return;
    }

    // check HDMI state
    ret = lt7911_get_signal_state(&signal_state);
    printk(KERN_INFO "HDMI signal state: 0x%02x\n", signal_state);
    if (ret < 0) {
        printk(KERN_ERR "Failed to get HDMI state\n");
        lt7911_disable();
        return;
    }

    if (signal_state & 0x01) {
        if (force_height != -1 && force_width != -1) {
            lt7911_force_resolution(force_width, force_height, force_fps);
        } else {
            hdmi_change_process(1); // HDMI signal is stable
        }
    } else {
        hdmi_change_process(0); // HDMI signal is disappearing
    }
    // if (signal_state & 0x01) {
    //     printk(KERN_INFO "HDMI signal is stable\n"); // HDMI signal is stable
    //     msleep(1000); // Simulated processing time
    // } else {
    //     printk(KERN_INFO "HDMI signal is disappearing\n"); // HDMI signal is disappearing
    // }

    // audio signal
    if (signal_state & 0x02) audio_change_process(1); // Audio signal is stable
    else                     audio_change_process(0); // Audio signal is disappearing
    // not to check sample rate go higher or lower
    // else if (signal_state & 0x04) audio_change_process(1); // Audio sample rate go higher
    // else                          audio_change_process(1); // Audio sample rate go lower

    if (lt7911_disable() < 0) return;

    return;
}

// Interupt IRQ
static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
    // read GPIO level
    int value = gpio_get_value(INT_PIN);

    if (!lt7911_init_done)
        return IRQ_HANDLED;

    if (chip_platform == LT7911_CHIP_LT7911D) {
        // LT7911D
        if (value == 1) {
            // rise edge detected: lt7911d
            schedule_work(&get_video_info_work);
        }
    } else {
        // printk(KERN_ERR "Unknown chip platform\n");
        return IRQ_HANDLED;
    }

    return IRQ_HANDLED;
}

static int lt7911_manage_probe(struct i2c_client *i2c,
                               const struct i2c_device_id *id)
{
    int ret;

    client = i2c;
    lt7911_init_done = false;
    INIT_WORK(&get_video_info_work, get_hdmi_info_handler);

    printk(KERN_INFO "Force HDMI width: %d, height: %d, fps: %d\n", force_width, force_height, force_fps);

    ret = lt7911_dt_parse(i2c);
    if (ret < 0) {
        printk(KERN_ERR "lt7911: dts parse failed\n");
        return ret;
    }

    // init GPIO
    ret = gpio_init();
    if (ret < 0) {
        printk(KERN_ERR "Failed to initialize GPIO\n");
        return -ENODEV;
    }

    // check chip register
    ret = check_chip_register();
    if (ret < 0) {
        printk(KERN_ERR "This module only supports LT7911 chip\n");
        gpio_exit();
        return -ENODEV;
    }

    // create proc info files
    if (proc_info_init() < 0) {
        printk(KERN_ERR "Failed to create proc info files\n");
        gpio_exit();
        return -ENOMEM;
    }

    // init proc info buffers
    proc_buffer_init();

    // read version from chip
    // if (lt7911_str_read(version_buffer) < 0) {
    //     return -EIO; // version read failed
    // }

    // read EDID from chip to buffer
    // video_edid_snapshot_buffer_length = EDID_BUFFER_SIZE;
    // if (lt7911_edid_read(video_edid_snapshot_buffer, video_edid_snapshot_buffer_length) < 0) {
    //     return -EIO; // EDID read failed
    // }

    // Restart Chip
    disable_irq(irq_number);
    lt7911_pwr_ctrl(0); // Power off the LT7911
    msleep(100);
    lt7911_pwr_ctrl(1); // Power on the LT7911 first
    msleep(100);
    enable_irq(irq_number);

    lt7911_init_done = true;

    printk(KERN_INFO "lt7911_manage module loaded\n");
    return 0;
}

static int lt7911_manage_remove(struct i2c_client *i2c)
{
    lt7911_init_done = false;
    cancel_work_sync(&get_video_info_work);
    proc_info_exit();
    gpio_exit();
    client = NULL;
    printk(KERN_INFO "GPIO-I2C interrupt module unloaded\n");

    return 0;
}

static const struct of_device_id lt7911_of_match[] = {
    { .compatible = "lontium,lt7911d" },
    { }
};
MODULE_DEVICE_TABLE(of, lt7911_of_match);

static const struct i2c_device_id lt7911_i2c_id[] = {
    { "lt7911d", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, lt7911_i2c_id);

static struct i2c_driver lt7911_manage_driver = {
    .driver = {
        .name = "lt7911_manage",
        .of_match_table = of_match_ptr(lt7911_of_match),
    },
    .probe = lt7911_manage_probe,
    .remove = lt7911_manage_remove,
    .id_table = lt7911_i2c_id,
};

static int __init lt7911_manage_init(void)
{
    return i2c_add_driver(&lt7911_manage_driver);
}

static void __exit lt7911_manage_exit(void)
{
    i2c_del_driver(&lt7911_manage_driver);
}

module_init(lt7911_manage_init);
module_exit(lt7911_manage_exit);

MODULE_LICENSE("GPL");
MODULE_VERSION("0.0.4");
MODULE_AUTHOR("Z2Z-BuGu");
MODULE_AUTHOR("916BGAI");
MODULE_DESCRIPTION("NanoAgent HDMI Module Management");
