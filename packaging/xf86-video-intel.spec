%define glamor 0

Name:           xf86-video-intel
Version:        2.20.10
Release:        0
License:        MIT
Summary:        Intel video driver for the Xorg X server
Url:            http://xorg.freedesktop.org/
Group:          System/X11/Servers/XF86_4
Source0:        %{name}-%{version}.tar.bz2
Source99:       baselibs.conf

%if %glamor
Requires:       glamor
%endif
BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  pkg-config
BuildRequires:  pkgconfig(dri2proto) >= 2.6
BuildRequires:  pkgconfig(fontsproto)
BuildRequires:  pkgconfig(gl)
BuildRequires:  pkgconfig(libdrm) >= 2.4.24
BuildRequires:  pkgconfig(libdrm_intel) >= 2.4.29
BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(pciaccess) >= 0.10
BuildRequires:  pkgconfig(pixman-1)
BuildRequires:  pkgconfig(randrproto)
BuildRequires:  pkgconfig(renderproto)
BuildRequires:  pkgconfig(resourceproto)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(x11-xcb)
BuildRequires:  pkgconfig(xcb-aux)
BuildRequires:  pkgconfig(xcb-dri2)
BuildRequires:  pkgconfig(xext)
BuildRequires:  pkgconfig(xextproto)
BuildRequires:  pkgconfig(xf86driproto)
BuildRequires:  pkgconfig(xfixes)
BuildRequires:  pkgconfig(xorg-macros) >= 1.8
BuildRequires:  pkgconfig(xorg-server) >= 1.0.99.901
BuildRequires:  pkgconfig(xproto)
BuildRequires:  pkgconfig(xrender)
BuildRequires:  pkgconfig(xvmc)
%if %glamor
BuildRequires:  pkgconfig(glamor)
%endif
ExclusiveArch:  %ix86 x86_64

%description
intel is an Xorg driver for Intel integrated video cards.

The driver supports depths 8, 15, 16 and 24. All visual types are
supported in depth 8. For the i810/i815 other depths support the
TrueColor and DirectColor visuals. For the i830M and later, only the
TrueColor visual is supported for depths greater than 8. The driver
supports hardware accelerated 3D via the Direct Rendering Infrastructure
(DRI), but only in depth 16 for the i810/i815 and depths 16 and 24 for
the 830M and later.

%prep
%setup -q

%build
autoreconf -fi
%configure \
	--enable-uxa \
	--enable-sna \
%if %glamor
        --enable-glamor \
%endif
	--with-default-accel=uxa
make %{?_smp_mflags}

%install
%make_install

%remove_docs

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%doc COPYING
%{_libdir}/libI810XvMC.so*
%{_libdir}/libIntelXvMC.so*
%dir %{_libdir}/xorg/modules/drivers
%{_libdir}/xorg/modules/drivers/intel_drv.so

%changelog
