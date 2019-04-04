# Camera
PRODUCT_PROPERTY_OVERRIDES += \
persist.camera.eis.enable=1 \
persist.camera.HAL3.enabled=1 \
vendor.camera.aux.packagelist=org.codeaurora.snapcam,com.android.camera,org.lineageos.snap \
vendor.camera.aux.packagelist2=com.google.android.GoogleCameraWide,com.dual.GCam,com.Wide.GCam,com.Tele.GCam \
persist.camera.dual.camera=0

# Always use GPU for screen compositing
PRODUCT_PROPERTY_OVERRIDES += \
debug.sf.disable_hwc=1


