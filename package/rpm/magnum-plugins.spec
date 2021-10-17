Name:           magnum-plugins
Version:        v2020.06
Release:        a3ab5c44%{?dist}
Summary:        Plugins for the Magnum C++11/C++14 graphics engine

License:        MIT
URL:            https://magnum.graphics/
Source0:        %{name}-%{version}-%{release}.tar.gz
Requires:       magnum
%if %{defined suse_version}
Requires:       libIL1
Requires:       faad2
Requires:       openexr
Requires:       assimp-devel
Requires:       libjpeg62
%else
Requires:       DevIL
Requires:       faad2-libs
Requires:       OpenEXR-libs
Requires:       assimp
Requires:       libjpeg-turbo
%endif
Requires:       openal-soft
Requires:       libpng
Requires:       freetype
BuildRequires:  gcc-c++
BuildRequires:  cmake >= 3.6.0
BuildRequires:  magnum-devel
BuildRequires:  DevIL-devel
BuildRequires:  openal-soft-devel
%if %{defined suse_version}
BuildRequires:  libjpeg62-devel
BuildRequires:  libfaad-devel
%else
BuildRequires:  libjpeg-turbo-devel
BuildRequires:  faad2-devel
%endif
BuildRequires:  libpng-devel
BuildRequires:  assimp-devel
BuildRequires:  freetype-devel
BuildRequires:  OpenEXR-devel

%description
Plugins for the Magnum C++11/C++14 graphics engine

%package devel
Summary:        Plugins for the Magnum C++11/C++14 graphics engine
Requires:       magnum-devel
Requires:       %{name} = %{version}

%description devel
Plugins for the Magnum C++11/C++14 graphics engine

%global debug_package %{nil}

%prep
%autosetup


%build
mkdir build && cd build
cmake .. \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_ASSIMPIMPORTER=ON \
    -DWITH_BASISIMAGECONVERTER=OFF \
    -DWITH_BASISIMPORTER=OFF \
    -DWITH_DDSIMPORTER=ON \
    -DWITH_DEVILIMAGEIMPORTER=ON \
    -DWITH_DRFLACAUDIOIMPORTER=ON \
    -DWITH_DRMP3AUDIOIMPORTER=ON \
    -DWITH_DRWAVAUDIOIMPORTER=ON \
    -DWITH_FAAD2AUDIOIMPORTER=ON \
    -DWITH_FREETYPEFONT=ON \
    -DWITH_GLSLANGSHADERCONVERTER=OFF \
    -DWITH_ICOIMPORTER=ON \
    -DWITH_JPEGIMAGECONVERTER=ON \
    -DWITH_JPEGIMPORTER=ON \
    -DWITH_KTXIMAGECONVERTER=ON \
    -DWITH_KTXIMPORTER=ON \
    -DWITH_MESHOPTIMIZERSCENECONVERTER=OFF \
    -DWITH_MINIEXRIMAGECONVERTER=ON \
    -DWITH_OPENEXRIMAGECONVERTER=ON \
    -DWITH_OPENEXRIMPORTER=ON \
    -DWITH_OPENGEXIMPORTER=ON \
    -DWITH_PNGIMAGECONVERTER=ON \
    -DWITH_PNGIMPORTER=ON \
    -DWITH_PRIMITIVEIMPORTER=ON \
    -DWITH_SPIRVTOOLSSHADERCONVERTER=OFF \
    -DWITH_STANFORDIMPORTER=ON \
    -DWITH_STANFORDSCENECONVERTER=ON \
    -DWITH_STBDXTIMAGECONVERTER=ON \
    -DWITH_STBIMAGECONVERTER=ON \
    -DWITH_STBIMAGEIMPORTER=ON \
    -DWITH_STBTRUETYPEFONT=ON \
    -DWITH_STBVORBISAUDIOIMPORTER=ON \
    -DWITH_STLIMPORTER=ON \
    -DWITH_TINYGLTFIMPORTER=ON
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
cd build
make DESTDIR=$RPM_BUILD_ROOT install
strip $RPM_BUILD_ROOT/%{_prefix}/lib*/lib*.so.* $RPM_BUILD_ROOT/%{_prefix}/lib*/magnum/*/*.so

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%{_prefix}/lib*/lib*.so.*
%{_prefix}/lib*/magnum/*/*
%doc COPYING

%files devel
%{_prefix}/include/Magnum/*
%{_prefix}/include/MagnumExternal/*
%{_prefix}/include/MagnumPlugins/*
%{_prefix}/lib*/lib*.so
%{_prefix}/share/cmake*
%doc COPYING


%changelog
* Sun Oct 17 2021 1b00 <1b00@pm.me> - v2020.06-a3ab5c44
- Initial release.
