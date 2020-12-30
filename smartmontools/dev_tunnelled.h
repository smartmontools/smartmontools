/*
 * dev_tunnelled.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2008-20 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DEV_TUNNELLED_H
#define DEV_TUNNELLED_H

#define DEV_TUNNELLED_H_CVSID "$Id$"

#include "dev_interface.h"

/////////////////////////////////////////////////////////////////////////////
// tunnelled_device_base

/// Common functionality for all tunnelled_device classes.

class tunnelled_device_base
: virtual public /*implements*/ smart_device
{
protected:
  explicit tunnelled_device_base(smart_device * tunnel_dev);

public:
  virtual ~tunnelled_device_base();

  virtual bool is_open() const;

  virtual bool open();

  virtual bool close();

  virtual bool owns(const smart_device * dev) const;

  virtual void release(const smart_device * dev);

private:
  smart_device * m_tunnel_base_dev;
};


/////////////////////////////////////////////////////////////////////////////
// tunnelled_device

/// Implement a device by tunneling through another device

template <class BaseDev, class TunnelDev>
class tunnelled_device
: public BaseDev,
  public tunnelled_device_base
{
public:
  typedef TunnelDev tunnel_device_type;

protected:
  explicit tunnelled_device(tunnel_device_type * tunnel_dev)
    : smart_device(smart_device::never_called),
      tunnelled_device_base(tunnel_dev),
      m_tunnel_dev(tunnel_dev)
    { }

  // For nvme_device
  explicit tunnelled_device(tunnel_device_type * tunnel_dev, unsigned nsid)
    : smart_device(smart_device::never_called),
      BaseDev(nsid),
      tunnelled_device_base(tunnel_dev),
      m_tunnel_dev(tunnel_dev)
    { }

public:
  virtual void release(const smart_device * dev)
    {
      if (m_tunnel_dev == dev)
        m_tunnel_dev = 0;
      tunnelled_device_base::release(dev);
    }

  tunnel_device_type * get_tunnel_dev()
    { return m_tunnel_dev; }

  const tunnel_device_type * get_tunnel_dev() const
    { return m_tunnel_dev; }

private:
  tunnel_device_type * m_tunnel_dev;
};

#endif // DEV_TUNNELLED_H
