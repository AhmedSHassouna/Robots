#pragma once

#include "DSAPIUtil.h"

#include <cstdlib>
#include <cstdint>  // For uint8_t, uint16_t, ...
#include <cstdarg>  // For va_list, va_start, ...
#include <cstdio>   // For vsnprintf
#include <ctime>    // For time_t, gmtime, strftime
#include <string>   // For std::string
#include <iostream> // For ostream, cerr
#include <iomanip>
#include <GL/freeglut.h> // For basic input/output

// DSAPI functions which can fail will return false.
// When this happens, DSAPI::getLastErrorStatus() will return an error code, and DSAPI::getLastErrorDescription() will return a human-readable detail string about what went wrong.
#define DS_CHECK_ERRORS(s)                                                                           \
    do                                                                                               \
    {                                                                                                \
        if (!s)                                                                                      \
        {                                                                                            \
            std::cerr << "\nDSAPI call failed at " << __FILE__ << ":" << __LINE__ << "!";            \
            std::cerr << "\n  status:  " << DSStatusString(g_dsapi->getLastErrorStatus());           \
            std::cerr << "\n  details: " << g_dsapi->getLastErrorDescription() << '\n' << std::endl; \
            std::cerr << "\n hit return to exit " << '\n' << std::endl;                              \
            getchar();                                                                               \
            exit(EXIT_FAILURE);                                                                      \
        }                                                                                            \
    } while (false);

// Simple utility class which computes a frame rate
class Timer
{
    int lastTime, frameCounter;
    float timeCounter, framesPerSecond;

public:
    Timer()
        : lastTime()
        , frameCounter()
        , timeCounter()
        , framesPerSecond()
    {
    }

    float GetFramesPerSecond() const { return framesPerSecond; }

    void OnFrame()
    {
        ++frameCounter;
        int time = glutGet(GLUT_ELAPSED_TIME);
        timeCounter += (time - lastTime) * 0.001f;
        lastTime = time;
        if (timeCounter >= 0.5f)
        {
            framesPerSecond = frameCounter / timeCounter;
            frameCounter = 0;
            timeCounter = 0;
        }
    }
};

// Produce an RGB image from a depth image by computing a cumulative histogram of depth values and using it to map each pixel between a near color and a far color
inline void ConvertDepthToRGBUsingHistogram(const uint16_t depthImage[], int width, int height, const uint8_t nearColor[3], const uint8_t farColor[3], uint8_t rgbImage[])
{
    // Produce a cumulative histogram of depth values
    int histogram[256 * 256] = {1};
    for (int i = 0; i < width * height; ++i)
    {
        if (auto d = depthImage[i]) ++histogram[d];
    }
    for (int i = 1; i < 256 * 256; i++)
    {
        histogram[i] += histogram[i - 1];
    }

    // Remap the cumulative histogram to the range 0..256
    for (int i = 1; i < 256 * 256; i++)
    {
        histogram[i] = (histogram[i] << 8) / histogram[256 * 256 - 1];
    }

    // Produce RGB image by using the histogram to interpolate between two colors
    auto rgb = rgbImage;
    for (int i = 0; i < width * height; i++)
    {
        if (uint16_t d = depthImage[i]) // For valid depth values (depth > 0)
        {
            auto t = histogram[d]; // Use the histogram entry (in the range of 0..256) to interpolate between nearColor and farColor
            *rgb++ = ((256 - t) * nearColor[0] + t * farColor[0]) >> 8;
            *rgb++ = ((256 - t) * nearColor[1] + t * farColor[1]) >> 8;
            *rgb++ = ((256 - t) * nearColor[2] + t * farColor[2]) >> 8;
        }
        else // Use black pixels for invalid values (depth == 0)
        {
            *rgb++ = 0;
            *rgb++ = 0;
            *rgb++ = 0;
        }
    }
}

// Print an non-rectified image's intrinsic properties to stdout
std::ostream & operator<<(std::ostream & out, const DSCalibIntrinsicsNonRectified & i)
{
    out << "\n  Image dimensions: " << i.w << ", " << i.h;
    out << "\n  Focal lengths: " << i.fx << ", " << i.fy;
    out << "\n  Principal point: " << i.px << ", " << i.py;
    return out << "\n  Distortion coefficients: " << i.k[0] << ", " << i.k[1] << ", " << i.k[2] << ", " << i.k[3] << ", " << i.k[4];
}

// Print a rectified image's intrinsic properties to stdout
std::ostream & operator<<(std::ostream & out, const DSCalibIntrinsicsRectified & i)
{
    float hFOV, vFOV;
    DSFieldOfViewsFromIntrinsicsRect(i, hFOV, vFOV);

    out << "\n  Image dimensions: " << i.rw << ", " << i.rh;
    out << "\n  Focal lengths: " << i.rfx << ", " << i.rfy;
    out << "\n  Principal point: " << i.rpx << ", " << i.rpy;
    return out << "\n  Field of view: " << hFOV << ", " << vFOV;
}

// Format a DSAPI timestamp in a human-readable fashion
std::string GetHumanTime(double secondsSinceEpoch)
{
    time_t time = (time_t)secondsSinceEpoch;
    char buffer[80];
    size_t i = strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", gmtime(&time));
    sprintf(buffer + i, ".%02d UTC", static_cast<int>(fmod(secondsSinceEpoch, 1.0) * 100));
    return buffer;
}

// Display a string using GLUT, with a 50% opaque black rectangle behind the text to make it stand out from bright backgrounds
inline void DrawString(int x, int y, const char * format, ...)
{
    // Format output string
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Compute string width in pixels
    int width = 0;
    for (auto s = buffer; *s; ++s)
    {
        width += glutBitmapWidth(GLUT_BITMAP_HELVETICA_12, *s);
    }

    // Set up a pixel-aligned orthographic coordinate space
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT), 0, -1, +1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Draw a 50% opaque black rectangle
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    glColor4f(0, 0, 0, 0.5f);
    glVertex2i(x, y);
    glVertex2i(x + width + 4, y);
    glVertex2i(x + width + 4, y + 16);
    glVertex2i(x, y + 16);
    glEnd();
    glDisable(GL_BLEND);

    // Draw the string using bitmap glyphs
    glColor3f(1, 1, 1);
    glRasterPos2i(x + 2, y + 12);
    for (auto s = buffer; *s; ++s)
    {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *s);
    }

    // Restore GL state to what it was prior to this call
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}


class GlutWindow
{
    int windowId;
    GLuint texture;
    static void OnDisplay() {}
    void Begin()
    {
        glutSetWindow(windowId);
        glViewport(0, 0, glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
        glPushAttrib(GL_ALL_ATTRIB_BITS);
    }
    void End()
    {
        glPopAttrib();
    }

public:
    GlutWindow()
        : windowId(), texture() {}

    void Open(std::string name, int width, int height, int startX, int startY, void (*keyboardFunc)(unsigned char key, int x, int y))
    {
        glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
        glutInitWindowSize(width, height);
        glutInitWindowPosition(startX, startY);
        windowId = glutCreateWindow(name.c_str());

        glutDisplayFunc(OnDisplay);
        glutKeyboardFunc(keyboardFunc);
    }

    void ClearScreen(float r, float g, float b)
    {
        Begin();
        glClearColor(r, g, b, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        End();
    }

    void DrawImage(GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid * pixels, GLfloat multiplier)
    {
        Begin();

        // Upload the image as a texture
        if (!texture) glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelTransferf(GL_RED_SCALE, multiplier);
        glPixelTransferf(GL_GREEN_SCALE, multiplier);
        glPixelTransferf(GL_BLUE_SCALE, multiplier);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, format, type, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // Draw the image via a scaled quad, as large as possible without modifying the aspect ratio of the image
        auto winWidth = glutGet(GLUT_WINDOW_WIDTH), winHeight = glutGet(GLUT_WINDOW_HEIGHT);
        auto scaleX = (GLfloat)winWidth / width, scaleY = (GLfloat)winHeight / height, scale = scaleX < scaleY ? scaleX : scaleY;
        auto vertX = width * scale / winWidth, vertY = height * scale / winHeight;
        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        glColor3f(multiplier, multiplier, multiplier);
        glTexCoord2f(0, 0);
        glVertex2f(-vertX, +vertY);
        glTexCoord2f(1, 0);
        glVertex2f(+vertX, +vertY);
        glTexCoord2f(1, 1);
        glVertex2f(+vertX, -vertY);
        glTexCoord2f(0, 1);
        glVertex2f(-vertX, -vertY);
        glEnd();

        End();
    }
};