
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/usb/input.h>
#define DRIVER_AUTHOR "Mathew Lin & Wang JiaWei& Chen HongJin"
//驱动作者
#define DRIVER_DESC "XBox 360 Controller Driver"
//驱动介绍
#define XPAD_PKT_LEN 32
//封包长度
/* xbox d-pads should map to buttons, as is required for DDR pads
   but we map them to axes when possible to simplify things */




static const struct xpad_device
{
    u16 idVendor; //供应商id
    u16 idProduct; //产品id
    char *name; //产品名字
} xpad_device[] =
{
    { 0x045e, 0x028e, "Microsoft XBox 360 Super Mouse" }
};


static const signed short xpad_common_btn[] =
{
    BTN_RIGHT, BTN_B, BTN_LEFT, BTN_Y,         //右 B 左 Y
    BTN_START, BTN_START, BTN_THUMBL, BTN_THUMBR,  /* start/back/sticks */
    -1                      //结束标志
};







static const signed short xpad360_btn[] =    
{
    KEY_LEFTALT, BTN_TR,        //LB RB
    BTN_MODE,       //微软键
    -1
};

static const signed short xpad_abs[] =
{
    ABS_X, ABS_Y,       //左轴
    ABS_RX, ABS_RY,     //右轴
    -1          /* terminating entry */
};



/* LT RT 映射成absz轴 */
static const signed short xpad_abs_triggers[] =
{
    ABS_Z, ABS_RZ,      /* triggers left/right */
    -1
};

#define XPAD_XBOX360_VENDOR_PROTOCOL(vend,pr) \
    .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_INT_INFO, \
                   .idVendor = (vend), \
                               .bInterfaceClass = USB_CLASS_VENDOR_SPEC, \
                                       .bInterfaceSubClass = 93, \
                                               .bInterfaceProtocol = (pr)
#define XPAD_XBOX360_VENDOR(vend) \
    { XPAD_XBOX360_VENDOR_PROTOCOL(vend,1) }


//注册设备
static struct usb_device_id xpad_table[] =
{
    { USB_INTERFACE_INFO('X', 'B', 0) },    /* X-Box USB-IF not approved class */
    XPAD_XBOX360_VENDOR(0x045e),        /* Microsoft X-Box 360 controllers */
    { }
};

MODULE_DEVICE_TABLE(usb, xpad_table);

struct usb_xpad
{
    struct input_dev *dev;      //输入设备接口
    struct usb_device *udev;    //usb设备
    struct usb_interface *intf; //usb接口

    int pad_present;

    struct urb *irq_in;    //用于终端报告的urb（USB Request Blocks）
    unsigned char *idata;       //输入数据
    dma_addr_t idata_dma; //输入数据的DMA接口地址

    struct urb *bulk_out; //用户手柄震动的urb
    unsigned char *bdata; //手柄震动的数据

    struct urb *irq_out;        //用于中断报告输出的urb（USB Request Blocks）
    unsigned char *odata;       //输出数据
    dma_addr_t odata_dma;   //输出数据的DMA接口地址
    struct mutex odata_mutex;  //互斥锁 用于同步

#if defined(CONFIG_JOYSTICK_XPAD_LEDS)
    struct xpad_led *led; //用于有led灯的设备结构
#endif

    char phys[64];          //设备的物理路径


 
};


//将raw（未加工的） packet 处理成 linux系统能识别的packet

static void xpad360_process_packet(struct usb_xpad *xpad,
                                   u16 cmd, unsigned char *data)
{
    int i;
    struct input_dev *dev = xpad->dev; //得到设备的输入设备接口

    //dpad 左右上下 的绑定
    input_report_key(dev, BTN_DPAD_LEFT, data[2] & 0x04); //绑定成游戏柄的左键
    input_report_key(dev, BTN_DPAD_RIGHT, data[2] & 0x08); //绑定成游戏柄的右键
    input_report_key(dev, KEY_UP, data[2] & 0x01);//绑定成键盘的上键
    input_report_key(dev, KEY_DOWN, data[2] & 0x02); //绑定成键盘的下键

    //开始 返回键的绑定
    input_report_key(dev, BTN_START,  data[2] & 0x10); //绑定成游戏柄的开始键
    input_report_key(dev, BTN_SELECT, data[2] & 0x20);//绑定成游戏柄的选择键

    //左右大拇指键
    input_report_key(dev, BTN_THUMBL, data[2] & 0x40); //绑定成左大拇指键

    input_report_key(dev, KEY_ENTER, data[2] & 0x80); //绑定成回车键

    // A,B,X,Y,TL,TR 和微软键
    input_report_key(dev, BTN_RIGHT, data[3] & 0x10);//绑定成鼠标右键

    input_report_key(dev, BTN_B,   data[3] & 0x20);//绑定成游戏柄B键


    input_report_key(dev, BTN_LEFT, data[3] & 0x40); //绑定成鼠标左键

    input_report_key(dev, BTN_Y ,  data[3] & 0x80);//绑定成游戏柄Y键
   
    input_report_key(dev, BTN_TL, (data[3] & 0x01)); //绑定成TL键
    
    input_report_key(dev, BTN_TR,   (data[3] & 0x02)); //绑定成TR键


    input_report_key(dev, BTN_MODE, data[3] & 0x04);


    //左游戏杆的绑定
    input_report_abs(dev, ABS_X,
                     ((__s16) le16_to_cpup((__le16 *)(data + 6))) );
    input_report_abs(dev, ABS_Y,
                     ((~(__s16) le16_to_cpup((__le16 *)(data + 8))) + 1)  );

    //右游戏杆的绑定
    input_report_abs(dev, ABS_RX,
                     (__s16) le16_to_cpup((__le16 *)(data + 10)));
    input_report_abs(dev, ABS_RY,
                     ((~(__s16) le16_to_cpup((__le16 *)(data + 12))) + 1) );


   //LT RT的绑定
        input_report_key(dev, BTN_TL2, !!data[4] ); //绑定成按键 （本来是0~255的范围 转化成 0~1）
  
        input_report_key(dev, BTN_TR2, !!data[5] ); //绑定成按键 （本来是0~255的范围 转化成 0~1）


    input_sync(dev); //将处理后的packet 提交上去
}





static void xpad_irq_in(struct urb *urb)  //检测 urb状态的函数
{
    struct usb_xpad *xpad = urb->context; //获取urb的内容  里面存放着usb_xpad的结构信息
    struct device *dev = &xpad->intf->dev; //根据usb设备接口获取该设备信息 intf为usb设备接口  dev是详细的设备信息
    int retval, status;

    status = urb->status; //urb的返回值

    switch (status) //判断urb的返回值
    {
    case 0:
        //成功
        break;
    case -ECONNRESET:
    case -ENOENT:
    case -ESHUTDOWN:
        //这个urb结束了
        dev_dbg(dev, "%s - urb shutting down with status: %d\n",
                __func__, status);
        return;
    default:
        dev_dbg(dev, "%s - nonzero urb status received: %d\n",
                __func__, status);
        goto exit;
    }


    xpad360_process_packet(xpad, 0, xpad->idata); //处理设备的raw packet


exit:
    retval = usb_submit_urb(urb, GFP_ATOMIC);//提交urb
    if (retval) //出错
        dev_err(dev, "%s - usb_submit_urb failed with result %d\n",
                __func__, retval);
}



static void xpad_irq_out(struct urb *urb) //检测 urb状态的函数
{
    struct usb_xpad *xpad = urb->context; //获取urb的内容  里面存放着usb_xpad的结构信息
    struct device *dev = &xpad->intf->dev;//根据usb设备接口获取该设备信息 intf为usb设备接口  dev是详细的设备信息
    int retval, status;

    status = urb->status;

    switch (status)
    {
    case 0:
        /* success */
        return;

    case -ECONNRESET:
    case -ENOENT:
    case -ESHUTDOWN:
        /* this urb is terminated, clean up */
        dev_dbg(dev, "%s - urb shutting down with status: %d\n",
                __func__, status);
        return;

    default:
        dev_dbg(dev, "%s - nonzero urb status received: %d\n",
                __func__, status);
        goto exit;
    }

exit:
    retval = usb_submit_urb(urb, GFP_ATOMIC);
    if (retval)
        dev_err(dev, "%s - usb_submit_urb failed with result %d\n",
                __func__, retval);
}

static int xpad_init_output(struct usb_interface *intf, struct usb_xpad *xpad) //初始化设备输出 
{
    struct usb_endpoint_descriptor *ep_irq_out;
    int ep_irq_out_idx;
    int error;

    xpad->odata = usb_alloc_coherent(xpad->udev, XPAD_PKT_LEN,
                                     GFP_KERNEL, &xpad->odata_dma); //输出类型的 DMA接口 数据  和 地址
    if (!xpad->odata)
    {
        error = -ENOMEM;
        goto fail1;
    }
    //初始化互斥锁 用于同步
    mutex_init(&xpad->odata_mutex);

    xpad->irq_out = usb_alloc_urb(0, GFP_KERNEL); //初始化 out型 urb
    if (!xpad->irq_out)
    {
        error = -ENOMEM;
        goto fail2;
    }

    /* Xbox One controller has in/out endpoints swapped. */
    ep_irq_out_idx =  1;
    ep_irq_out = &intf->cur_altsetting->endpoint[ep_irq_out_idx].desc;
	//函数结构 usb_fill_int_urb(struct urb* urb,struct  usb_device * dev,unsigned int pipe,void * transfer_buffer,int  buffer_length,usb_complete_t complete,void * context,int interval)
    //根据 usb设备，usb管道，输出缓冲区的首地址，缓冲区长度，out 型 urb入口函数，手柄设备数据信息，out endpoint口的轮换间隔信息 得到 输出型的urb
    usb_fill_int_urb(xpad->irq_out, xpad->udev,
                     usb_sndintpipe(xpad->udev, ep_irq_out->bEndpointAddress),
                     xpad->odata, XPAD_PKT_LEN,
                     xpad_irq_out, xpad, ep_irq_out->bInterval); 
    xpad->irq_out->transfer_dma = xpad->odata_dma; //分配DMA接口地址
    xpad->irq_out->transfer_flags |= URB_NO_TRANSFER_DMA_MAP; //允许 输出型urb 进行DMA传输

    return 0;

fail2: usb_free_coherent(xpad->udev, XPAD_PKT_LEN, xpad->odata, xpad->odata_dma);
fail1: return error;
}

static void xpad_stop_output(struct usb_xpad *xpad) //清空 out型 urb 的数据
{

    usb_kill_urb(xpad->irq_out);
}

static void xpad_deinit_output(struct usb_xpad *xpad)  //释放 out型 urb的空间
{

    usb_free_urb(xpad->irq_out);
    usb_free_coherent(xpad->udev, XPAD_PKT_LEN,
                      xpad->odata, xpad->odata_dma);

}





static int xpad_open(struct input_dev *dev)
{
    struct usb_xpad *xpad = input_get_drvdata(dev);  //根据输入设备 得到设备信息 即 返回usb_xpad类型的数据

    //提交usb 请求

    xpad->irq_in->dev = xpad->udev;
    if (usb_submit_urb(xpad->irq_in, GFP_KERNEL))
        return -EIO;

    return 0;
}

static void xpad_close(struct input_dev *dev)
{
    struct usb_xpad *xpad = input_get_drvdata(dev);  //根据输入设备 得到设备信息 即 返回usb_xpad类型的数据
    //清空 usb 请求
    usb_kill_urb(xpad->irq_in);

    xpad_stop_output(xpad);
}

static void xpad_set_up_abs(struct input_dev *input_dev, signed short abs) //注册绝对轴
{
    struct usb_xpad *xpad = input_get_drvdata(input_dev); //根据输入设备 得到设备信息 即 返回usb_xpad类型的数据
    set_bit(abs, input_dev->absbit); //注册abs轴

    switch (abs) //设定轴的取值范围
    {
    case ABS_X:
    case ABS_Y:
    case ABS_RX:
    case ABS_RY:    /* the two sticks */
        input_set_abs_params(input_dev, abs, -32768, 32767, 0, 0);
        break;
    case ABS_Z:
    case ABS_RZ:    /* the triggers (if mapped to axes) */
        input_set_abs_params(input_dev, abs, 0, 255, 0, 0);
        break;
    case ABS_HAT0X:
    case ABS_HAT0Y: /* the d-pad (only if dpad is mapped to axes */
        input_set_abs_params(input_dev, abs, -1, 1, 0, 0);
        break;
    }
}
static void xpad_set_up_rel(struct input_dev *input_dev, signed short abs) //注册相对轴
{
    struct usb_xpad *xpad = input_get_drvdata(input_dev);//根据输入设备 得到设备信息 即 返回usb_xpad类型的数据
    set_bit(abs, input_dev->relbit);//注册相对轴 相对轴不需要设定取值范围

}
//需要注册的其他按键
int key_need_register[] =
{
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_LEFTALT, KEY_RIGHTALT, KEY_TAB, KEY_ENTER, KEY_F7, BTN_START, BTN_BACK,BTN_TR, BTN_TR2,BTN_TL,BTN_TL2,
    BTN_SELECT,KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,KEY_6,KEY_7,KEY_8,KEY_9,KEY_0,
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,
    KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_EQUAL, KEY_MINUS, KEY_BACKSPACE, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_COMMA, KEY_DOT, KEY_SLASH,
    KEY_LEFTSHIFT,KEY_RIGHTSHIFT,KEY_LEFTCTRL,KEY_RIGHTCTRL,KEY_PAGEUP,KEY_PAGEDOWN,
    BTN_DPAD_LEFT,BTN_DPAD_RIGHT,KEY_F4,
    -1
};

static int xpad_probe(struct usb_interface *intf, const struct usb_device_id *id)  //探针函数 相当于设备驱动的主函数
{
    struct usb_device *udev = interface_to_usbdev(intf); //根据usb设备接口获取到usb设备  实际 返回的是 intf->dev.parent 即设备的父亲
    struct usb_xpad *xpad; //自己的定义的手柄设备类型
    struct input_dev *input_dev;//输入设备
    struct usb_endpoint_descriptor *ep_irq_in;  //in endpoint 的描述
    int ep_irq_in_idx;
    int i, error;

    for (i = 0; xpad_device[i].idVendor; i++) //根据实际插入的设备的生产商id和产品id  找到匹配的设备
    {
        if ((le16_to_cpu(udev->descriptor.idVendor) == xpad_device[i].idVendor) &&
                (le16_to_cpu(udev->descriptor.idProduct) == xpad_device[i].idProduct))
            break;
    }


    xpad = kzalloc(sizeof(struct usb_xpad), GFP_KERNEL); //在内核空间为手柄设备申请内存空间
    input_dev = input_allocate_device(); //为输入设备在内核空间申请内存空间  input_allocate_device()这个函数是用kzalloc函数申请了空间后 然后对该内存进行了初始化
    if (!xpad || !input_dev) //申请空间失败
    {
        error = -ENOMEM;
        goto fail1;
    }
    //usb_alloc_coherent (struct usb_device *dev,size_t size,gfp_t mem_flags,dma_addr_t *dma)--allocate dma-consistent buffer for URB_NO_xxx_DMA_MAP
     //分配DMA接口的缓冲区 返回 idata_dma  和 idata（raw packet，原始数据包）
    xpad->idata = usb_alloc_coherent(udev, XPAD_PKT_LEN,
                                     GFP_KERNEL, &xpad->idata_dma);
    if (!xpad->idata) //返回raw packet（原始数据包）失败
    {
        error = -ENOMEM;
        goto fail1;
    }
    //usb_alloc_urb (int iso_packets,gfp_t mem_flags)  create a new urb for a USB driver to use   iso_packets = 0 when use interrupt endpoints
    //创建新的urb给设备使用
    xpad->irq_in = usb_alloc_urb(0, GFP_KERNEL);
    if (!xpad->irq_in) //urb创建失败
    {
        error = -ENOMEM;
        goto fail2;
    }

    xpad->udev = udev; //保存usb设备信息
    xpad->intf = intf; //保存usb接口信息

    xpad->dev = input_dev;  //保存输入设备信息
    //创建物理路径
    usb_make_path(udev, xpad->phys, sizeof(xpad->phys));
    strlcat(xpad->phys, "/input0", sizeof(xpad->phys)); //input0类型的

    input_dev->name = xpad_device[i].name; //保存输入设备名字
    input_dev->phys = xpad->phys; //保存输入设备的物理路径
    usb_to_input_id(udev, &input_dev->id); //保存输入设备ID
    input_dev->dev.parent = &intf->dev; //保存输入设备的设备 输入设备的父亲是usb接口 ， usb接口的父亲 usb设备

    input_set_drvdata(input_dev, xpad);

    input_dev->open = xpad_open; //输入设备的打开函数
    input_dev->close = xpad_close; //输入设备的关闭函数
	//BIT_MASK(nr)  (1UL<<((nr)%BITS_PER_LONG）)
    input_dev->evbit[0] = BIT_MASK(EV_KEY);  //注册键盘事件

    input_dev->evbit[0] |= BIT_MASK(EV_REL); //注册相对轴事件
    input_dev->evbit[0] |= BIT_MASK(EV_ABS); //注册绝对轴事件
    /* set up axes */

    for (i = 0; xpad_abs[i] >= 0; i++) ///注册对应的绝对轴
        xpad_set_up_abs(input_dev, xpad_abs[i]);


    xpad_set_up_rel(input_dev, REL_WHEEL); //注册对应的相对轴
    for (i = 0 ; key_need_register[i] >= 0; i++) __set_bit(key_need_register[i], input_dev->keybit);
    //注册按键
    for (i = 0; xpad_common_btn[i] >= 0; i++)
        __set_bit(xpad_common_btn[i], input_dev->keybit);

    //注册游戏手柄按键

    for (i = 0; xpad360_btn[i] >= 0; i++)
        __set_bit(xpad360_btn[i], input_dev->keybit);

    
    //注册LT RT按键
    for (i = 0; xpad_abs_triggers[i] >= 0; i++)
        xpad_set_up_abs(input_dev, xpad_abs_triggers[i]);
    //初始化设备输出
    error = xpad_init_output(intf, xpad);
    if (error)
        goto fail3;


    ep_irq_in_idx =   0;
    ep_irq_in = &intf->cur_altsetting->endpoint[ep_irq_in_idx].desc; //根据usb接口得到 in endpoint口的描述信息
    //函数结构 usb_fill_int_urb(struct urb* urb,struct  usb_device * dev,unsigned int pipe,void * transfer_buffer,int  buffer_length,usb_complete_t complete,void * context,int interval)
    //根据 usb设备，usb管道，输入缓冲区的首地址，缓冲区长度，urb入口函数，手柄设备数据信息，in endpoint口的轮换间隔信息 得到 输入的urb
    usb_fill_int_urb(xpad->irq_in, udev,
                     usb_rcvintpipe(udev, ep_irq_in->bEndpointAddress),
                     xpad->idata, XPAD_PKT_LEN, xpad_irq_in,
                     xpad, ep_irq_in->bInterval);
    xpad->irq_in->transfer_dma = xpad->idata_dma; //传输的DMA接口地址
    xpad->irq_in->transfer_flags |= URB_NO_TRANSFER_DMA_MAP; //传输标志(允许DMA方式传输) urb->transfer_dma valid on submit
     //根据输入设备信息 注册设备
    error = input_register_device(xpad->dev);
    if (error)
        goto fail5;

    usb_set_intfdata(intf, xpad);



    return 0;


fail5: if (input_dev)
        input_ff_destroy(input_dev);   //free force feedback structures
fail4: xpad_deinit_output(xpad);  //free out urb and out dma
fail3: usb_free_urb(xpad->irq_in); //free in urb
fail2: usb_free_coherent(udev, XPAD_PKT_LEN, xpad->idata, xpad->idata_dma);  //free in dma
fail1: input_free_device(input_dev); //free input_dev
    kfree(xpad); //free xpad
    return error;

}

static void xpad_disconnect(struct usb_interface *intf) //设备断开函数
{
    struct usb_xpad *xpad = usb_get_intfdata (intf); //根据usb接口得到手柄设备信息

    input_unregister_device(xpad->dev);  //注销 该设备
    //释放内核空间
    xpad_deinit_output(xpad);   

    usb_free_urb(xpad->irq_in);  
    usb_free_coherent(xpad->udev, XPAD_PKT_LEN,
                      xpad->idata, xpad->idata_dma);  

    kfree(xpad->bdata);
    kfree(xpad);

    usb_set_intfdata(intf, NULL);
}
//标准usb驱动 表格  驱动信息  探针函数入口地址  断开函数入口地址
static struct usb_driver xpad_driver =
{
    .name       = "xpad",
    .probe      = xpad_probe,
    .disconnect = xpad_disconnect,
    .id_table   = xpad_table,
};

module_usb_driver(xpad_driver);
//驱动模块 作者 描述 许可
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
