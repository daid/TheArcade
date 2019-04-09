#ifndef CAMERA_CAPTURE_TEXTURE_H
#define CAMERA_CAPTURE_TEXTURE_H

#include <sp2/graphics/texture.h>
#include <sp2/io/cameraCapture.h>

class CameraCaptureTexture : public sp::OpenGLTexture
{
public:
    CameraCaptureTexture();
    
    bool open(int camera_index);
    void close();

    virtual void bind() override;
    
    sp::Image getFrame();
private:
    sp::io::CameraCapture* capture;
    int capture_delay = 0;
};

#endif//CAMERA_CAPTURE_TEXTURE_H

