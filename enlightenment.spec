%{!?_rel:%{expand:%%global _rel 0.enl%{?dist}}}

Summary: The Enlightenment window manager
Name: enlightenment
Version: 0.17.3
Release: %{_rel}
License: BSD
Group: User Interface/Desktops
URL: http://www.enlightenment.org/
Source: ftp://ftp.enlightenment.org/pub/enlightenment/%{name}-%{version}.tar.gz
Packager: %{?_packager:%{_packager}}%{!?_packager:Michael Jennings <mej@eterm.org>}
Vendor: %{?_vendorinfo:%{_vendorinfo}}%{!?_vendorinfo:The Enlightenment Project (http://www.enlightenment.org/)}
Distribution: %{?_distribution:%{_distribution}}%{!?_distribution:%{_vendor}}
Prefix: %{_prefix}
#BuildSuggests: xorg-x11-devel, XFree86-devel, libX11-devel
BuildRequires: efl-devel >= 1.7.6, edje-devel, edje-bin
BuildRequires: eeze-devel
Requires: efl >= 1.7.6
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
Enlightenment is a window manager.

%package devel
Summary: Development headers for Enlightenment. 
Group: User Interface/Desktops
Requires: %{name} = %{version}
Requires: efl-devel, edje-devel

%description devel
Development headers for Enlightenment.

%prep
%setup -q

%build
%{configure} --prefix=%{_prefix} --with-profile=FAST_PC
%{__make} %{?_smp_mflags} %{?mflags}

%install
%{__make} %{?mflags_install} DESTDIR=$RPM_BUILD_ROOT install
test -x `which doxygen` && sh gendoc || :
rm -f `find $RPM_BUILD_ROOT/usr/lib/enlightenment -name "*.a" -print`
rm -f `find $RPM_BUILD_ROOT/usr/lib/enlightenment -name "*.la" -print`

%clean
test "x$RPM_BUILD_ROOT" != "x/" && rm -rf $RPM_BUILD_ROOT

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%defattr(-, root, root)
%doc AUTHORS COPYING README
%dir %{_sysconfdir}/enlightenment
%config(noreplace) %{_sysconfdir}/enlightenment/*
%config(noreplace) %{_sysconfdir}/xdg/menus/enlightenment.menu
%{_bindir}/enlightenment
%{_bindir}/enlightenment_*
#%{_bindir}/eap_to_desktop
%{_libdir}/%{name}
%{_datadir}/%{name}
%{_datadir}/locale/*
%{_datadir}/xsessions/%{name}.desktop
%{_datadir}/applications/enlightenment_filemanager.desktop

%files devel
%defattr(-, root, root)
%{_includedir}/enlightenment
%{_libdir}/pkgconfig/*.pc

%changelog
