Summary: SILC Server
Name: silc-server
Version: 1.1.18
Release: 0.fc8
License: GPL
Group: Applications/Communications
URL: http://silcnet.org/
Source0: silc-server-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
BuildRequires: silc-toolkit-devel
Requires: silc-toolkit >= 1.1

%description
SILC (Secure Internet Live Conferencing) is a protocol which provides
secure conferencing services on the Internet over insecure channel.

%prep
%setup -q

%build
%configure --prefix=%{_prefix} \
           --mandir=%{_mandir} \
           --infodir=%{_infodir} \
           --bindir=%{_bindir} \
           --sbindir=%{_sbindir} \
           --datadir=%{_datadir} \
           --enable-ipv6
make -j4

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install
mv $RPM_BUILD_ROOT/%{_datadir}/doc/silc-server \
  $RPM_BUILD_ROOT/%{_datadir}/doc/silc-server-%version

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr (755,root,root,755)
%{_sbindir}/*
%defattr (644,root,root,755)
%{_sysconfdir}/silcd.conf
%{_sysconfdir}/silcalgs.conf
%{_mandir}/man5/*
%{_mandir}/man8/*
%doc %{_datadir}/doc

%changelog
* Sun Nov 18 2007 - Pekka Riikonen <priikone@silcnet.org>
- Initial version
