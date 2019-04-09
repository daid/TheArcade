#include "cameraCaptureTexture.h"

CameraCaptureTexture::CameraCaptureTexture()
: sp::OpenGLTexture(sp::Texture::Type::Dynamic, "CameraCaptureTexture")
{
    capture = nullptr;
}

bool CameraCaptureTexture::open(int camera_index)
{
    close();
    
    capture = new sp::io::CameraCapture(camera_index);
    if (!capture->isOpen())
    {
        close();
        return false;
    }
    return true;
}

void CameraCaptureTexture::close()
{
    if (capture)
        delete capture;
    capture = nullptr;
}

void CameraCaptureTexture::bind()
{
    if (capture)
    {
        if (capture_delay)
        {
            capture_delay--;
        }
        else
        {
            setImage(capture->getFrame());
            capture_delay = 2;
        }
    }
    sp::OpenGLTexture::bind();
}

sp::Image CameraCaptureTexture::getFrame()
{
    if (capture)
        return capture->getFrame();
    return sp::Image();
}
