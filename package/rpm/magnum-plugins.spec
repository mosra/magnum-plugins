Name: magnum-plugins
Version: 2020.06.1514.gccdbe941
Release: 1
Summary: Plugins for the Magnum C++11 graphics engine
License: MIT
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Requires: magnum, DevIL, libpng, libjpeg-turbo, freetype, assimp, glslang, spirv-tools-libs, libwebp, libspng, openexr-libs
BuildRequires: cmake, git, gcc-c++, DevIL-devel, libpng-devel, libjpeg-turbo-devel, freetype-devel, assimp-devel, harfbuzz-devel, glslang-devel, spirv-tools-devel, libwebp-devel, libspng-devel, openexr-devel
Source1: https://github.com/BinomialLLC/basis_universal/archive/refs/tags/v1_50_0_2.zip
Source2: https://github.com/zeux/meshoptimizer/archive/refs/tags/v0.22.zip

%description
Here are various plugins for the Magnum C++11 graphics engine - asset import
and conversion, text rendering and more.

%package devel
Summary: Magnum Plugins development files
Requires: %{name} = %{version}, magnum-devel

%description devel
Headers and tools needed for the Magnum plugins collection.

%prep
%setup -c -n %{name}-%{version}

%build
unzip %{SOURCE1} -d %{_builddir}/
unzip %{SOURCE2} -d %{_builddir}/

mv %{_builddir}/meshoptimizer-0.22 %{name}-%{version}/src/external/meshoptimizer

mkdir build && cd build
# Configure CMake
cmake ../%{name}-%{version} \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=%{_prefix} \
  -DBASIS_UNIVERSAL_DIR=%{_builddir}/basis_universal-1_50_0_2 \
  -DCMAKE_COLOR_DIAGNOSTICS=ON \
  -DMAGNUM_WITH_ASSIMPIMPORTER=ON \
  -DMAGNUM_WITH_ASTCIMPORTER=ON \
  -DMAGNUM_WITH_BASISIMAGECONVERTER=ON \
  -DMAGNUM_WITH_BASISIMPORTER=ON \
  -DMAGNUM_WITH_BCDECIMAGECONVERTER=ON \
  -DMAGNUM_WITH_DDSIMPORTER=ON \
  -DMAGNUM_WITH_DEVILIMAGEIMPORTER=ON \
  -DMAGNUM_WITH_DRFLACAUDIOIMPORTER=ON \
  -DMAGNUM_WITH_DRMP3AUDIOIMPORTER=ON \
  -DMAGNUM_WITH_DRWAVAUDIOIMPORTER=ON \
  -DMAGNUM_WITH_ETCDECIMAGECONVERTER=ON \
  -DMAGNUM_WITH_FAAD2AUDIOIMPORTER=OFF \
  -DMAGNUM_WITH_FREETYPEFONT=ON \
  -DMAGNUM_WITH_GLSLANGSHADERCONVERTER=ON \
  -DMAGNUM_WITH_GLTFIMPORTER=ON \
  -DMAGNUM_WITH_GLTFSCENECONVERTER=ON \
  -DMAGNUM_WITH_HARFBUZZFONT=ON \
  -DMAGNUM_WITH_ICOIMPORTER=ON \
  -DMAGNUM_WITH_JPEGIMAGECONVERTER=ON \
  -DMAGNUM_WITH_JPEGIMPORTER=ON \
  -DMAGNUM_WITH_KTXIMAGECONVERTER=ON \
  -DMAGNUM_WITH_KTXIMPORTER=ON \
  -DMAGNUM_WITH_LUNASVGIMPORTER=OFF \
  -DMAGNUM_WITH_MESHOPTIMIZERSCENECONVERTER=ON \
  -DMAGNUM_WITH_MINIEXRIMAGECONVERTER=ON \
  -DMAGNUM_WITH_OPENEXRIMAGECONVERTER=ON \
  -DMAGNUM_WITH_OPENEXRIMPORTER=ON \
  -DMAGNUM_WITH_OPENGEXIMPORTER=ON \
  -DMAGNUM_WITH_PLUTOSVGIMPORTER=OFF \
  -DMAGNUM_WITH_PNGIMAGECONVERTER=ON \
  -DMAGNUM_WITH_PNGIMPORTER=ON \
  -DMAGNUM_WITH_PRIMITIVEIMPORTER=ON \
  -DMAGNUM_WITH_RESVGIMPORTER=OFF \
  -DMAGNUM_WITH_SPIRVTOOLSSHADERCONVERTER=ON \
  -DMAGNUM_WITH_SPNGIMPORTER=ON \
  -DMAGNUM_WITH_STANFORDIMPORTER=ON \
  -DMAGNUM_WITH_STANFORDSCENECONVERTER=ON \
  -DMAGNUM_WITH_STBDXTIMAGECONVERTER=ON \
  -DMAGNUM_WITH_STBIMAGECONVERTER=ON \
  -DMAGNUM_WITH_STBIMAGEIMPORTER=ON \
  -DMAGNUM_WITH_STBRESIZEIMAGECONVERTER=ON \
  -DMAGNUM_WITH_STBTRUETYPEFONT=ON \
  -DMAGNUM_WITH_STBVORBISAUDIOIMPORTER=ON \
  -DMAGNUM_WITH_STLIMPORTER=ON \
  -DMAGNUM_WITH_UFBXIMPORTER=ON \
  -DMAGNUM_WITH_WEBPIMAGECONVERTER=ON \
  -DMAGNUM_WITH_WEBPIMPORTER=ON

make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd build
make DESTDIR=$RPM_BUILD_ROOT install
strip $RPM_BUILD_ROOT/%{_libdir}/*.so*
strip $RPM_BUILD_ROOT/%{_libdir}/magnum/*/*.so*

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%clean
rm -rf $RPM_BUILD_ROOT
rm -rf %{_builddir}/basis_universal-1_50_0_2

%files
%defattr(-,root,root,-)
%{_libdir}/*.so*
%{_libdir}/magnum/*/*

%doc %{name}-%{version}/COPYING

%files devel
%defattr(-,root,root,-)
%{_includedir}/Magnum
%{_includedir}/MagnumPlugins
%{_datadir}/cmake/MagnumPlugins

%changelog
# TODO: changelog
