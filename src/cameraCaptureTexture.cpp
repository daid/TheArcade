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
    if (capture->getState() != sp::io::CameraCapture::State::Streaming)
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
        sp::Image image(capture->getFrame());
        if (image.getSize().x > 0)
            setImage(std::move(image));
    }
    sp::OpenGLTexture::bind();
}

sp::Image CameraCaptureTexture::getFrame()
{
    if (capture)
        return capture->getFrame();
    return sp::Image();
}
