/*
 * DDC Driver Module
 *
 * Copyright 2007-2018 chengdu outwit.
 *
 * luoxiaofeng@superoutwit.com
 */
#define DEBUG
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/spi/spi.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/gpio/consumer.h>

#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>

#define DDC_OFFSET                              0x100000
#define COMMON_OFFSET                   0x180000
#define RF_CONTROL_OFFSET            0x130000
#define FX3_CONTROL_OFFSET          0x140000
#define TXDDR_OFFSET                         0x110000
#define CF_AXI_SPAN		                 (COMMON_OFFSET + 0x400)		   //span control offset address
//插值编号	半带滤波器插值倍数	CIC滤波器插值倍数	总插值倍数
//        0                        1	                                      1	                1
//        1                        2	                                      1	                2
//        2                        1	                                      3	                3
//        3                        4	                                      1	                4
//        4                        1	                                      5	                5
//        5                        2	                                      3	                6
//        6                        8	                                      1	                8
//        7                        2	                                      5	                10
//        8                        4	                                      3	                12
//        9                        1	                                      15                	15
//        10	              4	                                      4	                16
//        11	              4	                                      5	                20
//        12	              4	                                      6	                24
//        13	              2	                                      15	                30
//        14	              32                                           1	                32
//        15	              8	                                      5	                40
//        16	              8	                                      6	                48
//        17	              4	                                      15	                60
//        18	              32                                           2	                64
//        19	              16                                           5	                80
//        20	              16                                           6	                96
//        21	              8	                                      15	                120
//        22	              32                                           4	                128
//        23	              32                                           8	                256
//        24	              32                                           16	                512
#define DUC_INTERPOLATION            (COMMON_OFFSET + 0x408)                  //插值编号
#define HALFBAND_OFFSET                 (DDC_OFFSET + 0x0)
#define POLY_OFFSET                             (DDC_OFFSET + 0x400)
/*External temperature*/
#define CF_AXI_TEMP0		                 (COMMON_OFFSET+0x102*4)
#define CF_AXI_TEMP1		                 (COMMON_OFFSET+0x103*4)
#define CF_AXI_TEMP2		                 (COMMON_OFFSET+0x104*4)
/*ATT*/
#define RX1_ATT                                       (RF_CONTROL_OFFSET + 0)
#define RX2_ATT                                       (RF_CONTROL_OFFSET + 4)
#define TX1_ATT                                       (RF_CONTROL_OFFSET + 8)
#define TX2_ATT                                       (RF_CONTROL_OFFSET + 12)
/*HDL Version*/
#define HDL_USER_YEAR                       (COMMON_OFFSET + 0)
#define HDL_USER_MONTH                  (COMMON_OFFSET + 4)
#define HDL_USER_DATE                       (COMMON_OFFSET + 8)
#define HDL_USER_VERSION                (COMMON_OFFSET + 12)
/*FX3 USB3.0 control*/
#define FX3_MODE                                   (FX3_CONTROL_OFFSET +0x0)                 //FPGA program mode  default 1
#define FX3_LENTH                                  (FX3_CONTROL_OFFSET +0x4)                 //usb data length N-1  defalult 24*1024/4-1
#define FX3_INTERVAL                            (FX3_CONTROL_OFFSET +0x8)                  //  0 -> 122.88Msps 1 -> 61.44Msps  2->40.96Msps  and so on
#define FX3_DATA_EN                             (FX3_CONTROL_OFFSET +0xc)                   //default 1
#define FX3_RESET                                   (FX3_CONTROL_OFFSET +0x10)                //reset 0  -> 1

#define CONFIG_LNA                                (COMMON_OFFSET + 0x101*4)               //LNA  Control
#define CAPTURE_INTERVAL                 (COMMON_OFFSET + 0x180*4)
#define WORK_MODE                              (COMMON_OFFSET + 0x181*4)
#define GAIN_HB                                        (COMMON_OFFSET + 0x182*4)
#define GAIN_POLY                                   (COMMON_OFFSET + 0x183*4)
#define CALC_LEN                                      (COMMON_OFFSET + 0x184*4)
#define POW_LINE                                     (COMMON_OFFSET + 0x185*4)
#define POW_DB                                         (COMMON_OFFSET + 0x186*4)

static const int COEF_POLY[] = {
    0x0000 ,
    0xFFFE ,
    0x0006 ,
    0xFFF2 ,
    0x001F ,
    0xFFC5 ,
    0x006A ,
    0xFF4E ,
    0x0120 ,
    0xFE34 ,
    0x02F0 ,
    0xFA8A ,
    0x1389 ,
    0x0C88 ,
    0xFBBF ,
    0x023A ,
    0xFEB9 ,
    0x00BE ,
    0xFF95 ,
    0x0039 ,
    0xFFE4 ,
    0x000C ,
    0xFFFB ,
    0x0001 ,
    0x0000 ,
    0x0001 ,
    0xFFFD ,
    0x0009 ,
    0xFFEC ,
    0x0028 ,
    0xFFB9 ,
    0x0075 ,
    0xFF49 ,
    0x0114 ,
    0xFE66 ,
    0x0277 ,
    0xFB8A ,
    0x1894 ,
    0x053A ,
    0xFE30 ,
    0x00D1 ,
    0xFFA2 ,
    0x0024 ,
    0xFFF8 ,
    0xFFFB ,
    0x0008 ,
    0xFFF9 ,
    0x0004 ,
    0xFFFE ,
    0x0001 ,
    0x0001 ,
    0xFFFC ,
    0x0009 ,
    0xFFEE ,
    0x0021 ,
    0xFFCC ,
    0x004D ,
    0xFF96 ,
    0x0088 ,
    0xFF5C ,
    0x00BC ,
    0xFF35 ,
    0x1A6A ,
    0xFF35 ,
    0x00BC ,
    0xFF5C ,
    0x0088 ,
    0xFF96 ,
    0x004D ,
    0xFFCC ,
    0x0021 ,
    0xFFEE ,
    0x0009 ,
    0xFFFC ,
    0x0001 ,
    0x0001 ,
    0xFFFE ,
    0x0004 ,
    0xFFF9 ,
    0x0008 ,
    0xFFFB ,
    0xFFF8 ,
    0x0024 ,
    0xFFA2 ,
    0x00D1 ,
    0xFE30 ,
    0x053A ,
    0x1894 ,
    0xFB8A ,
    0x0277 ,
    0xFE66 ,
    0x0114 ,
    0xFF49 ,
    0x0075 ,
    0xFFB9 ,
    0x0028 ,
    0xFFEC ,
    0x0009 ,
    0xFFFD ,
    0x0001 ,
    0x0000 ,
    0x0001 ,
    0xFFFB ,
    0x000C ,
    0xFFE4 ,
    0x0039 ,
    0xFF95 ,
    0x00BE ,
    0xFEB9 ,
    0x023A ,
    0xFBBF ,
    0x0C88 ,
    0x1389 ,
    0xFA8A ,
    0x02F0 ,
    0xFE34 ,
    0x0120 ,
    0xFF4E ,
    0x006A ,
    0xFFC5 ,
    0x001F ,
    0xFFF2 ,
    0x0006 ,
    0xFFFE ,
    0x0000
};

static const int COEF_HB[] = {
    0x2        ,
    0xfff8     ,
    0x14       ,
    0xffd3     ,
    0x59       ,
    0xff5f     ,
    0x112      ,
    0xfe42     ,
    0x2c0      ,
    0xfbae     ,
    0x702      ,
    0xf322     ,
    0x2880     ,
    0x4000
};

struct cf_axi_ddc_state {
        unsigned int version;
        struct iio_info		iio_info;
        void __iomem		*regs;
        struct iio_dev 		*indio_dev;
        //ddc profile
        struct bin_attribute 	bin;
        char			  *bin_attr_buf;
        int                             *coef_poly;
        int                             *coef_hb;
        int                             coef_poly_num;
        int                             coef_hb_num;
        //tx ddr profile
        struct bin_attribute 	txddrbin;
        char			  *txddrbin_buf;
        unsigned int           startflag;
        unsigned int           enable_ddr_start_address;//default 0
        unsigned int           enable_ddr_end_address;//
        unsigned int           enable_ddr_flag;//0 disable , 1 enable
        unsigned int           enable_ddr_mode;//2--cycle,
        unsigned int           datalen;
        unsigned int           sequence;
        unsigned int           endflag;
};

static int cf_axi_ddc_read_raw(struct iio_dev *indio_dev,
                           struct iio_chan_spec const *chan,
                           int *val,
                           int *val2,
                           long m)
{
    struct cf_axi_ddc_state *st = iio_priv(indio_dev);
    int ret;

    mutex_lock(&indio_dev->mlock);
    switch (m) {
    case IIO_CHAN_INFO_RAW:
        switch (chan->type) {
            case IIO_TEMP:
                *val = ioread32(st->regs + chan->address) ;
                *val &= 0xFF;
                ret = IIO_VAL_INT;
                break;
            case IIO_VOLTAGE:
                *val = ioread32(st->regs + chan->address) ;
                ret = IIO_VAL_INT;
                break;
            default:
                ret = -EINVAL;
                break;
        }
        break;
    default:
        ret = -EINVAL;
        break;
    }
    mutex_unlock(&indio_dev->mlock);
    return ret;
}

static int cf_axi_ddc_write_raw(struct iio_dev *indio_dev,
                               struct iio_chan_spec const *chan,
                               int val,
                               int val2,
                               long mask)
{
    struct cf_axi_ddc_state *st = iio_priv(indio_dev);
    int ret = 0;
    int tmp = 0;

    mutex_lock(&indio_dev->mlock);
    switch (mask) {
    case IIO_CHAN_INFO_RAW:
        switch (chan->type) {
            case IIO_TEMP:
                break;
            case IIO_VOLTAGE:
                if(chan->address == DUC_INTERPOLATION)
                {
                    tmp = ioread32(st->regs + TXDDR_OFFSET) ;
                    if(tmp)
                    {
                        iowrite32(0, st->regs + TXDDR_OFFSET);
                    }
                }
                iowrite32(val, st->regs + chan->address);
                if(tmp)
                {
                    iowrite32(st->enable_ddr_mode, st->regs + TXDDR_OFFSET);
                }
                break;
            default:
                ret = -EINVAL;
                break;
        }
        break;
    default:
        ret = -EINVAL;
        break;
    }
    mutex_unlock(&indio_dev->mlock);
    return ret;
}

static int cf_axi_ddc_reg_access(struct iio_dev *indio_dev,
                              unsigned reg, unsigned writeval,
                              unsigned *readval)
{
    return 0;
}

static int cf_axi_ddc_update_scan_mode(struct iio_dev *indio_dev,
        const unsigned long *scan_mask)
{
    return 0;
}

static int ddc_parse_profile(struct cf_axi_ddc_state *st,
                                 char *data, u32 size)
{
    char *line, *ptr = data;
    char  header = 0,coefpoly = 0,coefhb = 0;
    int num = 0,ret,retval = 0;
    while ((line = strsep(&ptr, "\n"))) {
            if (line >= data + size) {
                    break;
            }
            line = skip_spaces(line);

            if (line[0] == '#' || line[0] == '\r' ||  line[0] == '\0')
                    continue;

            if (!header && strstr(line, "<profile DDC")) {
                    header = 1;
                    continue;
            }

            if(!coefpoly && strstr(line, "<coef-poly>")){
                coefpoly = 1;
                num = 0;
                continue;
            }
            if(coefpoly && strstr(line, "</coef-poly>")){
                coefpoly = 0;
                st->coef_poly_num = num;
                num = 0;
                continue;
            }
            if(!coefhb&& strstr(line, "<coef-hb>")){
                coefhb = 1;
                num = 0;
                continue;
            }
            if(coefhb&& strstr(line, "</coef-hb>")){
                coefhb = 0;
                st->coef_hb_num = num;
                num = 0;
                continue;
            }
            if(coefpoly){
                if (st->coef_poly == NULL) {
                        st->coef_poly = devm_kzalloc(&st->indio_dev->dev,
                                                1024 * sizeof(*st->coef_poly), GFP_KERNEL);
                }
                if(sscanf(line, "0x%x", &ret) == 1)
                {
                    if(num >= 1024){
                        dev_err(&st->indio_dev->dev, "%s:%d: Invalid number (%d) of coefficients poly",
                                __func__, __LINE__, num);
                        return -EINVAL;
                    }
                    st->coef_poly[num++] = ret;
                    continue;
                }
            }
            if(coefhb){
                if(st->coef_hb == NULL){
                    st->coef_hb = devm_kzalloc(&st->indio_dev->dev,
                                            256 * sizeof(*st->coef_hb), GFP_KERNEL);
                }
                if(sscanf(line, "0x%x", &ret) == 1)
                {
                    if(num >= 256){
                        dev_err(&st->indio_dev->dev, "%s:%d: Invalid number (%d) of coefficients hb",
                                __func__, __LINE__, num);
                        return -EINVAL;
                    }
                    st->coef_hb[num++] = ret;
                    continue;
                }
            }
            if (header && strstr(line, "</profile>")) {
                    return retval;
            }

            /* We should never end up here */
            dev_err(&st->indio_dev->dev, "%s: Malformed profile entry was %s",
                    __func__, line);

            return -EINVAL;
    }
    return -EINVAL;
}

static ssize_t
ddc_profile_bin_write(struct file *filp, struct kobject *kobj,
                       struct bin_attribute *bin_attr,
                       char *buf, loff_t off, size_t count)
{
    struct iio_dev *indio_dev = dev_to_iio_dev(kobj_to_dev(kobj));
    struct cf_axi_ddc_state *st = iio_priv(indio_dev);
    int ret = 0,i;
    if (off == 0) {
            if (st->bin_attr_buf == NULL) {
                    st->bin_attr_buf = devm_kzalloc(&indio_dev->dev,
                                            bin_attr->size, GFP_KERNEL);
                    if (!st->bin_attr_buf)
                    {
                            dev_err(&indio_dev->dev, "%s:st->bin_attr_buf devm_kzalloc error", __func__);
                            return -ENOMEM;
                    }
            } else {
                    memset(st->bin_attr_buf, 0, bin_attr->size);
            }
    }
    memcpy(st->bin_attr_buf + off, buf, count);
    if (strnstr(st->bin_attr_buf, "</profile>", off + count) == NULL)
    {
            dev_err(&indio_dev->dev, "%s:strnstr st->bin_attr_buf </profile> error", __func__);
            return count;
    }
    ret = ddc_parse_profile(st, st->bin_attr_buf, off + count);
    if (ret < 0)
    {
            dev_err(&indio_dev->dev, "%s:ddc_parse_profile return error", __func__);
            return ret;
    }
    for(i = 0; i < st->coef_hb_num; i++)
    {
         iowrite32(st->coef_hb[i],st->regs + HALFBAND_OFFSET + i*4);
    }
    for(i = 0; i < st->coef_poly_num; i++)
    {
        iowrite32(st->coef_poly[i],st->regs + POLY_OFFSET + i*4);
    }
    return (ret < 0) ? ret : count;
}

static ssize_t
ddc_profile_bin_read(struct file *filp, struct kobject *kobj,
                       struct bin_attribute *bin_attr,
                       char *buf, loff_t off, size_t count)
{
    if (off)
            return 0;

    return sprintf(buf, "TBD");
}

static ssize_t
txddr_profile_bin_write(struct file *filp, struct kobject *kobj,
                       struct bin_attribute *bin_attr,
                       char *buf, loff_t off, size_t count)
{
    struct iio_dev *indio_dev = dev_to_iio_dev(kobj_to_dev(kobj));
    struct cf_axi_ddc_state *st = iio_priv(indio_dev);
    int ret = 0,i;
    int *data = NULL;
    unsigned int endflag = 0;
    unsigned int startflag = 0;

    if (off == 0) {
            if (st->txddrbin_buf == NULL) {
                    st->txddrbin_buf = devm_kzalloc(&indio_dev->dev,
                                            bin_attr->size, GFP_KERNEL);
                    if (!st->txddrbin_buf)
                    {
                            dev_err(&indio_dev->dev, "%s:st->bin_attr_buf devm_kzalloc error", __func__);
                            return -ENOMEM;
                    }
            } else {
                    memset(st->txddrbin_buf, 0, bin_attr->size);
            }
    }
    memcpy(st->txddrbin_buf + off, buf, count);

    memcpy(&startflag, st->txddrbin_buf, sizeof(unsigned int));
    if(startflag != st->startflag)
    {
            dev_err(&indio_dev->dev, "%s:st->startflag error", __func__);
            return count;
    }
    memcpy(&st->enable_ddr_flag,st->txddrbin_buf+sizeof(unsigned int), sizeof(unsigned int));
    memcpy(&st->enable_ddr_mode,st->txddrbin_buf+sizeof(unsigned int)*2, sizeof(unsigned int));
    memcpy(&st->enable_ddr_start_address,st->txddrbin_buf+sizeof(unsigned int)*3, sizeof(unsigned int));
    memcpy(&st->enable_ddr_end_address,st->txddrbin_buf+sizeof(unsigned int)*4, sizeof(unsigned int));
    memcpy(&st->datalen, st->txddrbin_buf+sizeof(unsigned int)*5, sizeof(unsigned int));
    if(st->datalen % 4 != 0)
    {
            dev_err(&indio_dev->dev, "%s:st->datalen mod 4 != 0", __func__);
            return count;
    }
    if(st->datalen > bin_attr->size - sizeof(unsigned int)*4)
    {
            dev_err(&indio_dev->dev, "%s:st->datalen = %d , error", __func__,st->datalen);
            return count;
    }

    memcpy(&endflag, st->txddrbin_buf +sizeof(unsigned int) * 7 +  st->datalen, sizeof(unsigned int));
    if(endflag != st->endflag)
    {
            //dev_err(&indio_dev->dev, "%s:st->endflag = 0x%08X , error", __func__,endflag);
            return count;
    }
    iowrite32(0,st->regs + TXDDR_OFFSET + 0x00);
    //dev_info(&indio_dev->dev, "%s:st->endflag = 0x%08X , success", __func__,endflag);
    if(st->datalen ==0)
    {
        goto enable_txddr;
    }
    memcpy(&st->sequence, st->txddrbin_buf+sizeof(unsigned int)*6, sizeof(unsigned int));
    if(st->sequence <= 0)
    {
            dev_err(&indio_dev->dev, "%s:st->sequence == %d, st->sequence must >=1.", __func__,st->sequence);
            return count;
    }
    data  = (int *)(st->txddrbin_buf + sizeof(unsigned int) * 7);

    for(i = 0; i < st->datalen/sizeof(int)/4; i++)
    {
        iowrite32(data[i*4],st->regs + TXDDR_OFFSET + 0x10);
        iowrite32(data[i*4 + 1],st->regs + TXDDR_OFFSET + 0x0c);
        iowrite32(data[i*4 + 2],st->regs + TXDDR_OFFSET + 0x08);
        iowrite32(data[i*4 + 3],st->regs + TXDDR_OFFSET + 0x04);
        iowrite32(i,st->regs + TXDDR_OFFSET + 0x14);
        iowrite32(1,st->regs + TXDDR_OFFSET + 0x18);
        iowrite32(0,st->regs + TXDDR_OFFSET + 0x18);
    }
    iowrite32(0x20000 * st->sequence,st->regs + TXDDR_OFFSET + 0x1c);
    iowrite32(1,st->regs + TXDDR_OFFSET + 0x20);
    iowrite32(0,st->regs + TXDDR_OFFSET + 0x20);
    i = 10;
    while(1)
    {
        if(ioread32(st->regs +TXDDR_OFFSET + 0x2c ))
            break;
        msleep(1);
        i--;
        if(i ==0)
        {
            dev_err(&indio_dev->dev, "%s:ioread32(st->regs +TXDDR_OFFSET + 0x2c ) error", __func__);
            return count;
        }
    }
enable_txddr:
    if(st->enable_ddr_flag)
    {
            iowrite32(st->enable_ddr_start_address,st->regs + TXDDR_OFFSET + 0x30);
            if(0 == st->enable_ddr_end_address)
            {
                iowrite32(0x20000 * (st->sequence - 1) + st->datalen/2,st->regs + TXDDR_OFFSET + 0x28);
            }else{
                iowrite32(st->enable_ddr_end_address,st->regs + TXDDR_OFFSET + 0x28);
            }
            iowrite32(st->enable_ddr_mode,st->regs + TXDDR_OFFSET + 0x00);
            iowrite32(1,st->regs + TXDDR_OFFSET + 0x24);
            iowrite32(0,st->regs + TXDDR_OFFSET + 0x24);
    }

    return (ret < 0) ? ret : count;
}

static ssize_t
txddr_profile_bin_read(struct file *filp, struct kobject *kobj,
                       struct bin_attribute *bin_attr,
                       char *buf, loff_t off, size_t count)
{
    if (off)
            return 0;

    return sprintf(buf, "TBD");
}

static ssize_t cf_axi_span_write(struct iio_dev *indio_dev,
                                   uintptr_t private,
                                   const struct iio_chan_spec *chan,
                                   const char *buf, size_t len)
{
    struct cf_axi_ddc_state *st = iio_priv(indio_dev);
    int ret = 0;
    u64 readin;
    //dev_dbg(&indio_dev->dev,"enter cf_axi_span_write.\n");
    ret = kstrtoull(buf, 10, &readin);
    if (ret)
            return ret;
    mutex_lock(&indio_dev->mlock);
    switch (private) {
    case 0:
            if(readin >= 100000000)
                iowrite32(0x0, st->regs + CF_AXI_SPAN);
            else if(readin >= 80000000)
                iowrite32(0x1, st->regs + CF_AXI_SPAN);
            else if(readin >= 50000000)
                iowrite32(0x2, st->regs + CF_AXI_SPAN);
            else if(readin >= 20000000)
                iowrite32(0x3, st->regs + CF_AXI_SPAN);
            else if(readin >= 10000000)
                iowrite32(0x4, st->regs + CF_AXI_SPAN);
            else if(readin >= 5000000)
                iowrite32(0x5, st->regs + CF_AXI_SPAN);
            else if(readin >= 2000000)
                iowrite32(0x6, st->regs + CF_AXI_SPAN);
            else if(readin >= 1000000)
                iowrite32(0x7, st->regs + CF_AXI_SPAN);
            else if(readin >= 500000)
                iowrite32(0x8, st->regs + CF_AXI_SPAN);
            else if(readin >= 200000)
                iowrite32(0x9, st->regs + CF_AXI_SPAN);
            else
                iowrite32(0xa, st->regs + CF_AXI_SPAN);
            break;
    default:
        ret = -EINVAL;
        break;
    }
    mutex_unlock(&indio_dev->mlock);
    return ret ? ret : len;
}

static ssize_t cf_axi_span_read(struct iio_dev *indio_dev,
                                  uintptr_t private,
                                  const struct iio_chan_spec *chan,
                                  char *buf)
{
    struct cf_axi_ddc_state *st = iio_priv(indio_dev);
    int ret = 0;
    u64 val = 0;
    unsigned int temp;
    //dev_dbg(&indio_dev->dev,"enter cf_axi_span_read.\n");
    mutex_lock(&indio_dev->mlock);
    switch (private) {
    case 0:
            temp =  ioread32(st->regs + CF_AXI_SPAN);
            if(temp == 0x00000)
                val = 100000000;
            else if(temp == 0x1)
                val = 80000000;
            else if(temp == 0x2)
                val = 50000000;
            else if(temp == 0x3)
                val = 20000000;
            else if(temp == 0x4)
                val = 10000000;
            else if(temp == 0x5)
                val = 5000000;
            else if(temp == 0x6)
                val = 2000000;
            else if(temp == 0x7)
                val = 1000000;
            else if(temp == 0x8)
                val = 500000;
            else if(temp == 0x9)
                val = 200000;
            else if(temp == 0xa)
                val = 100000;
            ret = 0;
            break;
    default:
            ret = 0;

    }
    mutex_unlock(&indio_dev->mlock);
    return ret ? ret : sprintf(buf, "%llu\n", val);
}

#define _AD9371_EXT_LO_INFO(_name, _ident) { \
        .name = _name, \
        .read = cf_axi_span_read, \
        .write = cf_axi_span_write, \
        .private = _ident, \
}

static const struct iio_chan_spec_ext_info cf_axi_span_ext_info[] = {
        /* Ideally we use IIO_CHAN_INFO_FREQUENCY, but there are
         * values > 2^32 in order to support the entire frequency range
         * in Hz. Using scale is a bit ugly.
         */
        _AD9371_EXT_LO_INFO("frequency", 0),
        { },
};

static const struct iio_chan_spec cf_axi_ddc_channels[] = {
        {
                .type = IIO_ALTVOLTAGE,
                .indexed = 1,
                .output = 1,
                .channel = 0,
                .extend_name = "SPAN",
                .ext_info = cf_axi_span_ext_info,
        },{
                .type = IIO_TEMP,
                .indexed = 1,
                .channel = 0,
                .address = CF_AXI_TEMP0,
                .extend_name = "local",
                .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        }, {
                .type = IIO_TEMP,
                .indexed = 1,
                .channel = 1,
                .address = CF_AXI_TEMP1,
                .extend_name = "remote",
                .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        }, {
                .type = IIO_TEMP,
                .indexed = 1,
                .channel = 2,
                .address = CF_AXI_TEMP2,
                .extend_name = "status",
                .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 0,
            .address = RX1_ATT,
            .extend_name = "RX1_ATT",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 1,
            .address = RX2_ATT,
            .extend_name = "RX2_ATT",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 2,
            .address = TX1_ATT,
            .extend_name = "TX1_ATT",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 3,
            .address = TX2_ATT,
            .extend_name = "TX2_ATT",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 4,
            .address = CONFIG_LNA ,
            .extend_name = "LNA",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 5,
            .address = CAPTURE_INTERVAL  ,
            .extend_name = "CAPTURE_INTERVAL ",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 6,
            .address = WORK_MODE ,
            .extend_name = "WORK_MODE",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 7,
            .address = GAIN_HB  ,
            .extend_name = "GAIN_HB ",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 8,
            .address = GAIN_POLY  ,
            .extend_name = "GAIN_POLY ",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 9,
            .address = FX3_MODE ,
            .extend_name = "FX3_MODE",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 10,
            .address = FX3_LENTH ,
            .extend_name = "FX3_LENTH",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 11,
            .address = FX3_INTERVAL ,
            .extend_name = "FX3_INTERVAL",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 12,
            .address = FX3_DATA_EN ,
            .extend_name = "FX3_DATA_EN",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 13,
            .address = FX3_RESET ,
            .extend_name = "FX3_RESET",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 14,
            .address = CALC_LEN ,
            .extend_name = "CALC_LEN",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 15,
            .address = POW_LINE ,
            .extend_name = "POW_LINE",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 16,
            .address = POW_DB ,
            .extend_name = "POW_DB",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        },{
            .type = IIO_VOLTAGE,
            .indexed = 1,
            .channel = 17,
            .address = DUC_INTERPOLATION ,
            .extend_name = "duc_interpolation",
            .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) ,
        }
};

static const struct iio_info cf_axi_ddc_info = {
        .driver_module = THIS_MODULE,
        .read_raw = &cf_axi_ddc_read_raw,
        .write_raw = &cf_axi_ddc_write_raw,
        .debugfs_reg_access = &cf_axi_ddc_reg_access,
        .update_scan_mode = &cf_axi_ddc_update_scan_mode,
};

static const struct of_device_id cf_axi_ddc_of_match[] = {
        { .compatible = "adi,axi-p9032ddc-1.00.a"},
        { },
};

static int cf_axi_ddc_probe(struct platform_device *pdev)
{
        struct device_node *np = pdev->dev.of_node;
        const struct of_device_id *id;
        struct cf_axi_ddc_state *st;
        struct iio_dev *indio_dev;
        struct resource *res;
        int ret ,i;
        unsigned int year,month,date,version;

        dev_dbg(&pdev->dev,"enter cf_axi_ddc_probe.\n");
        id = of_match_device(cf_axi_ddc_of_match, &pdev->dev);
        if (!id)
        {
            dev_dbg(&pdev->dev,"id = null.\n");
                return -ENODEV;
        }

        dev_dbg(&pdev->dev, "Device Tree Probing \'%s\'\n",
                        np->name);

        indio_dev = iio_device_alloc(sizeof(*st));
        if (!indio_dev)
        {
            dev_dbg(&pdev->dev,"indio_dev = null.\n");
                return -ENOMEM;
        }
        dev_dbg(&pdev->dev,"iio_device_alloc success.\n");

        st = iio_priv(indio_dev);
        st->indio_dev = indio_dev;

        res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        dev_dbg(&pdev->dev,"platform_get_resource success.\n");

        st->regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
        if (!st->regs) {
            dev_dbg(&pdev->dev,"st->regs = null.\n");
                ret = -ENOMEM;
                goto err_iio_device_free;
        }
        dev_dbg(&pdev->dev,"devm_ioremap success.\n");
        iowrite32(0x00000, st->regs + CF_AXI_SPAN);

        st->version = ioread32(st->regs + CF_AXI_SPAN);
        dev_dbg(&pdev->dev,"st->version = %d.\n",st->version);
        year = ioread32(st->regs + HDL_USER_YEAR);
        month = ioread32(st->regs + HDL_USER_MONTH);
        date = ioread32(st->regs + HDL_USER_DATE);
        version = ioread32(st->regs + HDL_USER_VERSION);
        dev_info(&pdev->dev,"HDL USER datetime %x/%x/%x.version(%x)",year,month,date,version);

        iowrite32(0, st->regs + RX1_ATT);
        iowrite32(0, st->regs + RX2_ATT);
        iowrite32(0, st->regs + TX1_ATT);
        iowrite32(0, st->regs + TX2_ATT);
        iowrite32(0, st->regs + CONFIG_LNA);

        for(i = 0; i < sizeof(COEF_HB)/sizeof(COEF_HB[0]) ; i++)
        {
            iowrite32(COEF_HB[i],st->regs + HALFBAND_OFFSET + i*4);
        }

        for(i = 0; i < sizeof(COEF_POLY)/sizeof(COEF_POLY[0]) ; i++)
        {
            iowrite32(COEF_POLY[i],st->regs + POLY_OFFSET + i*4);
        }

        indio_dev->dev.parent = &pdev->dev;
        indio_dev->name = np->name;
        st->iio_info = cf_axi_ddc_info;
        indio_dev->info = &st->iio_info;
        indio_dev->modes = INDIO_DIRECT_MODE;
        indio_dev->channels = cf_axi_ddc_channels;
        indio_dev->num_channels = ARRAY_SIZE(cf_axi_ddc_channels);

        sysfs_bin_attr_init(&st->bin);
        st->bin.attr.name = "ddc_profile";
        st->bin.attr.mode = S_IWUSR | S_IRUGO;
        st->bin.write = ddc_profile_bin_write;
        st->bin.read = ddc_profile_bin_read;
        st->bin.size = 8192;

        st->bin_attr_buf = NULL;
        st->coef_poly = NULL;
        st->coef_hb = NULL;

        sysfs_bin_attr_init(&st->txddrbin);
        st->txddrbin.attr.name = "txddr_profile";
        st->txddrbin.attr.mode = S_IWUSR | S_IRUGO;
        st->txddrbin.write = txddr_profile_bin_write;
        st->txddrbin.read = txddr_profile_bin_read;
        st->txddrbin.size = 0x40100;//0x40010 actual

        st->txddrbin_buf = NULL;
        st->startflag = 0xDDDDDDDD;
        st->datalen = 0;
        st->sequence = 0;
        st->endflag = 0xEEEEEEEE;

        dev_dbg(&pdev->dev,"enter iio_device_register.\n");
        ret = iio_device_register(indio_dev);
        if (ret)
                goto err_unconfigure_buffer;

        ret = sysfs_create_bin_file(&indio_dev->dev.kobj, &st->bin);
        if (ret < 0)
                goto out_iio_device_unregister;
        ret = sysfs_create_bin_file(&indio_dev->dev.kobj, &st->txddrbin);
        if (ret < 0)
                goto out_iio_device_unregister;

        platform_set_drvdata(pdev, indio_dev);

        return 0;

out_iio_device_unregister:
        iio_device_unregister(indio_dev);
err_unconfigure_buffer:
        //iio_dmaengine_buffer_free(indio_dev->buffer);

err_iio_device_free:
        iio_device_free(indio_dev);

        return ret;
}

static int cf_axi_ddc_remove(struct platform_device *pdev)
{
        struct iio_dev *indio_dev = platform_get_drvdata(pdev);
        //struct cf_axi_ddc_state *st = iio_priv(indio_dev);

        iio_device_unregister(indio_dev);
        iio_device_free(indio_dev);

        return 0;
}

static struct platform_driver cf_axi_ddc_driver = {
        .driver = {
                .name = "cf_axi_ddc",
                .owner = THIS_MODULE,
                .of_match_table = cf_axi_ddc_of_match,
        },
        .probe		= cf_axi_ddc_probe,
        .remove		= cf_axi_ddc_remove,
};
module_platform_driver(cf_axi_ddc_driver);

MODULE_AUTHOR("lxf<luoxiaofeng@superoutwit.com>");
MODULE_DESCRIPTION("Control AXI DDC");
MODULE_LICENSE("GPL v2");
