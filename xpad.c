
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/usb/input.h>
#define DRIVER_AUTHOR "Mathew Lin & Wang JiaWei& Chen HongJin"
#define DRIVER_DESC "XBox 360 Controller Driver"

#define XPAD_PKT_LEN 32

/* xbox d-pads should map to buttons, as is required for DDR pads
   but we map them to axes when possible to simplify things */
#define MAP_DPAD_TO_BUTTONS     (1 << 0)

#define XTYPE_XBOX360     1

static bool dpad_to_buttons;
module_param(dpad_to_buttons, bool, S_IRUGO);
MODULE_PARM_DESC(dpad_to_buttons, "Map D-PAD to buttons rather than axes for unknown pads");


static const struct xpad_device
{
    u16 idVendor;
    u16 idProduct;
    char *name;
    u8 mapping;
    u8 xtype;
} xpad_device[] =
{
    { 0x045e, 0x028e, "Microsoft XBox 360 Super Mouse", 0, XTYPE_XBOX360 }
};

/* buttons shared with xbox and xbox360 */
static const signed short xpad_common_btn[] =
{
    BTN_RIGHT, BTN_B, BTN_LEFT, BTN_Y,          /* "analog" buttons */
    BTN_START, BTN_START, BTN_THUMBL, BTN_THUMBR,  /* start/back/sticks */
    -1                      /* terminating entry */
};


/* used when dpad is mapped to buttons */
static const signed short xpad_btn_pad[] =
{
    KEY_LEFT, KEY_RIGHT,        /* d-pad left, right */
    KEY_UP, KEY_DOWN,       /* d-pad up, down */
    -1              /* terminating entry */
};

/* used when triggers are mapped to buttons */
static const signed short xpad_btn_triggers[] =
{
    BTN_TL2, BTN_TR2,       /* triggers left/right */
    -1
};


static const signed short xpad360_btn[] =    /* buttons for x360 controller */
{
    KEY_LEFTALT, BTN_TR,        /* Button LB/RB */
    BTN_MODE,       /* The big X button */
    -1
};

static const signed short xpad_abs[] =
{
    ABS_X, ABS_Y,       /* left stick */
    ABS_RX, ABS_RY,     /* right stick */
    -1          /* terminating entry */
};

/* used when dpad is mapped to axes */
static const signed short xpad_abs_pad[] =
{
    ABS_HAT0X, ABS_HAT0Y,   /* d-pad axes */
    -1          /* terminating entry */
};

/* used when triggers are mapped to axes */
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



static struct usb_device_id xpad_table[] =
{
    { USB_INTERFACE_INFO('X', 'B', 0) },    /* X-Box USB-IF not approved class */
    XPAD_XBOX360_VENDOR(0x045e),        /* Microsoft X-Box 360 controllers */
    { }
};

MODULE_DEVICE_TABLE(usb, xpad_table);

struct usb_xpad
{
    struct input_dev *dev;      /* input device interface */
    struct usb_device *udev;    /* usb device */
    struct usb_interface *intf; /* usb interface */

    int pad_present;

    struct urb *irq_in;     /* urb for interrupt in report */
    unsigned char *idata;       /* input data */
    dma_addr_t idata_dma;

    struct urb *bulk_out;
    unsigned char *bdata;

    struct urb *irq_out;        /* urb for interrupt out report */
    unsigned char *odata;       /* output data */
    dma_addr_t odata_dma;
    struct mutex odata_mutex;

#if defined(CONFIG_JOYSTICK_XPAD_LEDS)
    struct xpad_led *led;
#endif

    char phys[64];          /* physical device path */

    int mapping;            /* map d-pad to buttons or to axes */
    int xtype;          /* type of xbox device */
};


/*
 *  xpad360_process_packet
 *
 *  Completes a request by converting the data into events for the
 *  input subsystem. It is version for xbox 360 controller
 *
 *  The used report descriptor was taken from:
 *      http://www.free60.org/wiki/Gamepad
 */


static void xpad360_process_packet(struct usb_xpad *xpad,
                                   u16 cmd, unsigned char *data)
{
    int i;
    struct input_dev *dev = xpad->dev;

    /* digital pad */
    /* dpad as buttons (left, right, up, down) */
    input_report_key(dev, BTN_DPAD_LEFT, data[2] & 0x04);
    input_report_key(dev, BTN_DPAD_RIGHT, data[2] & 0x08);
    input_report_key(dev, KEY_UP, data[2] & 0x01);
    input_report_key(dev, KEY_DOWN, data[2] & 0x02);

    /* start/back buttons */
    input_report_key(dev, BTN_START,  data[2] & 0x10);
    input_report_key(dev, BTN_SELECT, data[2] & 0x20);

    /* stick press left/right */
    input_report_key(dev, BTN_THUMBL, data[2] & 0x40);

    input_report_key(dev, KEY_ENTER, data[2] & 0x80);

    /* buttons A,B,X,Y,TL,TR and MODE */
    input_report_key(dev, BTN_RIGHT, data[3] & 0x10);

    input_report_key(dev, BTN_B,   data[3] & 0x20);


    input_report_key(dev, BTN_LEFT, data[3] & 0x40);
    //input_report_key(dev, KEY_LEFTALT , data[3] & 0x80);
    input_report_key(dev, BTN_Y ,  data[3] & 0x80);
   
    input_report_key(dev, BTN_TL, (data[3] & 0x01));
    
    input_report_key(dev, BTN_TR,   (data[3] & 0x02));
    // input_report_key(dev, KEY_RIGHTALT, data[3] & 0x02);
    //input_report_key(dev, KEY_F7,   data[3] & 0x02);


    input_report_key(dev, BTN_MODE, data[3] & 0x04);


    /* left stick */
    input_report_abs(dev, ABS_X,
                     ((__s16) le16_to_cpup((__le16 *)(data + 6))) );
    input_report_abs(dev, ABS_Y,
                     ((~(__s16) le16_to_cpup((__le16 *)(data + 8))) + 1)  );

    /* right stick */
    input_report_abs(dev, ABS_RX,
                     (__s16) le16_to_cpup((__le16 *)(data + 10)));
    input_report_abs(dev, ABS_RY,
                     ((~(__s16) le16_to_cpup((__le16 *)(data + 12))) + 1) );


    /* triggers left/right */
   //if ((unsigned char )data[4] > 0)
        input_report_key(dev, BTN_TL2, !!data[4] );
    //else
        input_report_key(dev, BTN_TR2, !!data[5] );


    input_sync(dev);
}





static void xpad_irq_in(struct urb *urb)
{
    struct usb_xpad *xpad = urb->context;
    struct device *dev = &xpad->intf->dev;
    int retval, status;

    status = urb->status;

    switch (status)
    {
    case 0:
        /* success */
        break;
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


    xpad360_process_packet(xpad, 0, xpad->idata);


exit:
    retval = usb_submit_urb(urb, GFP_ATOMIC);
    if (retval)
        dev_err(dev, "%s - usb_submit_urb failed with result %d\n",
                __func__, retval);
}

static void xpad_bulk_out(struct urb *urb)
{
    struct usb_xpad *xpad = urb->context;
    struct device *dev = &xpad->intf->dev;

    switch (urb->status)
    {
    case 0:
        /* success */
        break;
    case -ECONNRESET:
    case -ENOENT:
    case -ESHUTDOWN:
        /* this urb is terminated, clean up */
        dev_dbg(dev, "%s - urb shutting down with status: %d\n",
                __func__, urb->status);
        break;
    default:
        dev_dbg(dev, "%s - nonzero urb status received: %d\n",
                __func__, urb->status);
    }
}

static void xpad_irq_out(struct urb *urb)
{
    struct usb_xpad *xpad = urb->context;
    struct device *dev = &xpad->intf->dev;
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

static int xpad_init_output(struct usb_interface *intf, struct usb_xpad *xpad)
{
    struct usb_endpoint_descriptor *ep_irq_out;
    int ep_irq_out_idx;
    int error;

    xpad->odata = usb_alloc_coherent(xpad->udev, XPAD_PKT_LEN,
                                     GFP_KERNEL, &xpad->odata_dma);
    if (!xpad->odata)
    {
        error = -ENOMEM;
        goto fail1;
    }

    mutex_init(&xpad->odata_mutex);

    xpad->irq_out = usb_alloc_urb(0, GFP_KERNEL);
    if (!xpad->irq_out)
    {
        error = -ENOMEM;
        goto fail2;
    }

    /* Xbox One controller has in/out endpoints swapped. */
    ep_irq_out_idx =  1;
    ep_irq_out = &intf->cur_altsetting->endpoint[ep_irq_out_idx].desc;

    usb_fill_int_urb(xpad->irq_out, xpad->udev,
                     usb_sndintpipe(xpad->udev, ep_irq_out->bEndpointAddress),
                     xpad->odata, XPAD_PKT_LEN,
                     xpad_irq_out, xpad, ep_irq_out->bInterval);
    xpad->irq_out->transfer_dma = xpad->odata_dma;
    xpad->irq_out->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    return 0;

fail2: usb_free_coherent(xpad->udev, XPAD_PKT_LEN, xpad->odata, xpad->odata_dma);
fail1: return error;
}

static void xpad_stop_output(struct usb_xpad *xpad)
{

    usb_kill_urb(xpad->irq_out);
}

static void xpad_deinit_output(struct usb_xpad *xpad)
{

    usb_free_urb(xpad->irq_out);
    usb_free_coherent(xpad->udev, XPAD_PKT_LEN,
                      xpad->odata, xpad->odata_dma);

}





static int xpad_open(struct input_dev *dev)
{
    struct usb_xpad *xpad = input_get_drvdata(dev);

    /* URB was submitted in probe */

    xpad->irq_in->dev = xpad->udev;
    if (usb_submit_urb(xpad->irq_in, GFP_KERNEL))
        return -EIO;

    return 0;
}

static void xpad_close(struct input_dev *dev)
{
    struct usb_xpad *xpad = input_get_drvdata(dev);

    usb_kill_urb(xpad->irq_in);

    xpad_stop_output(xpad);
}

static void xpad_set_up_abs(struct input_dev *input_dev, signed short abs)
{
    struct usb_xpad *xpad = input_get_drvdata(input_dev);
    set_bit(abs, input_dev->absbit);

    switch (abs)
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
static void xpad_set_up_rel(struct input_dev *input_dev, signed short abs)
{
    struct usb_xpad *xpad = input_get_drvdata(input_dev);
    set_bit(abs, input_dev->relbit);

}
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

static int xpad_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(intf);
    struct usb_xpad *xpad;
    struct input_dev *input_dev;
    struct usb_endpoint_descriptor *ep_irq_in;
    int ep_irq_in_idx;
    int i, error;

    for (i = 0; xpad_device[i].idVendor; i++)
    {
        if ((le16_to_cpu(udev->descriptor.idVendor) == xpad_device[i].idVendor) &&
                (le16_to_cpu(udev->descriptor.idProduct) == xpad_device[i].idProduct))
            break;
    }


    xpad = kzalloc(sizeof(struct usb_xpad), GFP_KERNEL);
    input_dev = input_allocate_device();
    if (!xpad || !input_dev)
    {
        error = -ENOMEM;
        goto fail1;
    }
    //usb_alloc_coherent (struct usb_device *dev,size_t size,gfp_t mem_flags,dma_addr_t *dma)--allocate dma-consistent buffer for URB_NO_xxx_DMA_MAP
    xpad->idata = usb_alloc_coherent(udev, XPAD_PKT_LEN,
                                     GFP_KERNEL, &xpad->idata_dma);
    if (!xpad->idata)
    {
        error = -ENOMEM;
        goto fail1;
    }
    //usb_alloc_urb (int iso_packets,gfp_t mem_flags)  create a new urb for a USB driver to use   iso_packets = 0 when use interrupt endpoints
    xpad->irq_in = usb_alloc_urb(0, GFP_KERNEL);
    if (!xpad->irq_in)
    {
        error = -ENOMEM;
        goto fail2;
    }

    xpad->udev = udev;
    xpad->intf = intf;
    xpad->mapping = xpad_device[i].mapping;
    xpad->xtype = xpad_device[i].xtype;

    xpad->dev = input_dev;
    //creat phys path
    usb_make_path(udev, xpad->phys, sizeof(xpad->phys));
    strlcat(xpad->phys, "/input0", sizeof(xpad->phys));

    input_dev->name = xpad_device[i].name;
    input_dev->phys = xpad->phys;
    usb_to_input_id(udev, &input_dev->id);
    input_dev->dev.parent = &intf->dev;

    input_set_drvdata(input_dev, xpad);

    input_dev->open = xpad_open;
    input_dev->close = xpad_close;

    input_dev->evbit[0] = BIT_MASK(EV_KEY);

    input_dev->evbit[0] |= BIT_MASK(EV_REL);
    input_dev->evbit[0] |= BIT_MASK(EV_ABS);
    /* set up axes */

    for (i = 0; xpad_abs[i] >= 0; i++) ///!!!!!!!!!!
        xpad_set_up_abs(input_dev, xpad_abs[i]);


    //input_dev->evbit[0] |= BIT_MASK(EV_REL);
    xpad_set_up_rel(input_dev, REL_WHEEL);
    //xpad_set_up_rel(input_dev, REL_Y);
    for (i = 0 ; key_need_register[i] >= 0; i++) __set_bit(key_need_register[i], input_dev->keybit);
    /* set up standard buttons */
    for (i = 0; xpad_common_btn[i] >= 0; i++)
        __set_bit(xpad_common_btn[i], input_dev->keybit);

    /* set up model-specific ones */

    for (i = 0; xpad360_btn[i] >= 0; i++)
        __set_bit(xpad360_btn[i], input_dev->keybit);

    // for (i = 0; xpad_abs_pad[i] >= 0; i++)
    //  xpad_set_up_abs(input_dev, xpad_abs_pad[i]);

    for (i = 0; xpad_abs_triggers[i] >= 0; i++)
        xpad_set_up_abs(input_dev, xpad_abs_triggers[i]);

    error = xpad_init_output(intf, xpad);
    if (error)
        goto fail3;


    ep_irq_in_idx =   0;
    ep_irq_in = &intf->cur_altsetting->endpoint[ep_irq_in_idx].desc;
    //usb_fill_int_urb(struct urb* urb,struct  usb_device * dev,unsigned int pipe,void * transfer_buffer,int  buffer_length,usb_complete_t complete,void * context,int interval)
    usb_fill_int_urb(xpad->irq_in, udev,
                     usb_rcvintpipe(udev, ep_irq_in->bEndpointAddress),
                     xpad->idata, XPAD_PKT_LEN, xpad_irq_in,
                     xpad, ep_irq_in->bInterval);
    xpad->irq_in->transfer_dma = xpad->idata_dma;
    xpad->irq_in->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

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

static void xpad_disconnect(struct usb_interface *intf)
{
    struct usb_xpad *xpad = usb_get_intfdata (intf);

    input_unregister_device(xpad->dev);  //unregister input_device
    xpad_deinit_output(xpad);   //free out urb and out dma

    usb_free_urb(xpad->irq_in);  //free in urb
    usb_free_coherent(xpad->udev, XPAD_PKT_LEN,
                      xpad->idata, xpad->idata_dma);  //free in dma

    kfree(xpad->bdata);
    kfree(xpad);

    usb_set_intfdata(intf, NULL);
}

static struct usb_driver xpad_driver =
{
    .name       = "xpad",
    .probe      = xpad_probe,
    .disconnect = xpad_disconnect,
    .id_table   = xpad_table,
};

module_usb_driver(xpad_driver);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
