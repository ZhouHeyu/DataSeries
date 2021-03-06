#!/bin/sh
set -e
[ -d "$1" ]
TMP_USR=$1/usr

echo 'MESSAGE(${CMAKE_ROOT})' >$TMP_USR/tmp.cmake
CMAKE_MODULE_PATH=`cmake -P $TMP_USR/tmp.cmake 2>&1`/Modules
rm $TMP_USR/tmp.cmake
[ -d $CMAKE_MODULE_PATH ]
CMAKE_MODULE_SUBPATH=`echo $CMAKE_MODULE_PATH | sed 's,^/usr,,'`
[ -d /usr/$CMAKE_MODULE_SUBPATH ]
mkdir -p $TMP_USR/$CMAKE_MODULE_SUBPATH
mv $TMP_USR/share/cmake-modules/* $TMP_USR/$CMAKE_MODULE_SUBPATH
rmdir $TMP_USR/share/cmake-modules

# Unclear whether lexgrog or doxygen is wrong, but lexgrog does not accept spaces 
# on the left hand side of the NAME entry.
find $TMP_USR/share/man/man3 -type f -print0 | xargs -0 perl -i -p -e 'if ($fix) { ($left, $right) = /^(.+)( \\-.+)$/; $left =~ s/ //go; $_ = "$left$right\n"; $fix = 0; }; $fix = 1 if /^.SH NAME/o;' 

for i in $TMP_USR/bin/* $TMP_USR/lib/libDataSeries.so.*; do
    TYPE=`file $i`
    case "$TYPE" in
        *dynamically*linked*) chrpath -d $i; strip $i ;;
        *ELF*LSB*shared*object*) chrpath -d $i; strip $i ;;
        *ASCII*text*|*shell*script*|*[Pp]erl*script*) : ;;
        *symbolic*link*to*) : ;;
        *) echo "File '$i' has unknown type '$TYPE'"; exit 1 ;;
    esac
done

case $TMP_USR in
    *debian*) MAKE_OVERRIDES=true ;;
    *) MAKE_OVERRIDES=false ;;
esac

if $MAKE_OVERRIDES; then
    OVERRIDES=debian/tmp/usr/share/lintian/overrides
    mkdir -p $OVERRIDES
    for i in `grep \^Package: debian/control | awk '{print $2}'`; do
        echo "Fixing lintian override for $i"
        cat <<EOF >$OVERRIDES/$i 
# The only GPL code is lindump-mmap which does not link with openssl
# override lintians 'wild guess'
$i binary: possible-gpl-code-linked-with-openssl
EOF
    done
fi
