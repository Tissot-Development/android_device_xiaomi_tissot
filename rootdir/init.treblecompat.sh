#! /vendor/bin/sh

# common functions
symlink() {
	ln -sn $1 $2;
}

# mount system (root for our device) and vendor as rw
mount -o remount,rw /
mount -o remount,rw /vendor


# symlink camera
symlink /vendor/etc/morpho_lowlight4.0.xml /system/etc/morpho_lowlight4.0.xml
symlink /vendor/lib/libMiCameraHal.so /system/lib/libMiCameraHal.so
symlink /vendor/lib/libdualcameraddm.so /system/lib/libdualcameraddm.so
symlink /vendor/lib/libjni_dualcamera.so /system/lib/libjni_dualcamera.so
symlink /vendor/lib/libmorpho_groupshot.so /system/lib/libmorpho_groupshot.so
symlink /vendor/lib/libmorpho_memory_allocator.so /system/lib/libmorpho_memory_allocator.so
symlink /vendor/lib/libmorpho_panorama.so /system/lib/libmorpho_panorama.so
symlink /vendor/lib/libmorphohht4.0.so /system/lib/libmorphohht4.0.so
symlink /vendor/lib/libtrueportrait.so /system/lib/libtrueportrait.so
if [ ! -d "/system/etc/camera" ]; then mkdir /system/etc/camera; fi
chmod 755 /system/etc/camera
cd /vendor/etc/camera/
for f in *; do
	symlink /vendor/etc/camera/$f /system/etc/camera/$f
done

# remount system and vendor as ro
mount -o remount,ro /
mount -o remount,ro /vendor