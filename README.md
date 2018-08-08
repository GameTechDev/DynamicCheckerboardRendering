# Checkerboard Rendering (CBR) and Dynamic Resolution Rendering (DRR) Sample in DX12 MiniEngine
This repository is based on a snapshot of Microsoft's repository.

## Sample Overview
*The tested IDE is Visual Studio 2017.*

This repository contains a straight forward integration of CBR and DRR into the DX12 MiniEngine.  An indepth explanation of CBR and the algorithms presented here can be found in the corresponding white paper: [TODO: add white paper link]

The sample is designed to "build and run".  Simply clone (or download) this repository, build the MiniEngine\ModelViewer\ModelViewer_VS17.sln solution and run.  CBR is enabled by default, it (along with multiple other post processes) can be enabled or disabled by pressing the 'backspace' key to bring up the toggle menu.

### CBR
The CBR options in the toggle menu are as follows:
* **Enable**: Enable or disable CBR.
* **Check Shading Occlusion**: During movement which spans multiple pixels per frame: If enabled; Attempt to retrieve the correct shading information from frame N-1's pixels.  If disabled; Always assume the shading is occluded and extract the shading color from frame N.
* **Show Derived Motion**: For each pixel which had motion derived from the depth buffer, render it as red in the color buffer.
* **Show Missing Pixels**: For each pixel which our algorithm determined the shading information was missing, render it as red in the color buffer.
* **Show Occluded Pixels**: For each pixel which our algorithm determined the shading information was occluded by movement, render it as hot-pink in the color buffer.
* **Show Pixel Motion**: For each pixel which our algorithm determined motion vectors must be used to fetch the shading information from frame N-1, render it as green in the color buffer.
* **Depth Tolerance**: The depth tolerance (linear from near clip to far clip) used by our algorithm to determine if the shading information is occluded.

### DRR
The DRR options in the toggle menu are as follows:
* **Enable**: Enable or disable DRR.
* **Desired Frame Rate**: The target frame rate for the DRR algorithm.
* **Force Scale**: Force DRR to run at the scale specified by Min Scale.
* **Min Scale**: The low end limit to DRR's resolution scaling.
* **Resolution Increments**: The increments DRR will use when scaling resolutions.
* **_Advanced/Frame Rate Delta Resolution**: The amount of 'delta' (or headroom) to allow in the frame rate before switching resolutions.
* **_Advanced/Frame Rate Low Pass K**: The K filtering value used when accumulating the latest frame time.
* **_Advanced/Rate of Change**: The rate at which DRR's internal scale changes.
* **_Internal/Frame Rate**: The frame rate DRR is tracking, this is filtered by the value specified with 'Frame Rate Low Pass K'.
* **_Internal/Scale**: DRR's internal scale, once this passes the 'Resolution Increment' threshold the resolution will change. 



### Files
The core of CBR and DRR exists in two files:

#### ModelViewer.cpp
Modified version of the original ModelViewer.cpp in the MiniEngine.  This file is responsible for jittering the viewport and setting the correct render states.  To find the CBR specific code do a search for "CBR", to find the DRR specific code do a search for "DRR".

#### CheckerboardColorResolveCS.cs
This file is the CBR reconstruction shader, the crux of the CBR algorithm, it constructs a full resolution image from frames N-1 and N.



# DirectX-Graphics-Samples
This repo contains the DirectX Graphics samples that demonstrate how to build graphics intensive applications for Windows 10. We also have a YouTube channel! Visit us here: https://www.youtube.com/MicrosoftDirectX12andGraphicsEducation

## API Samples
In the Samples directory, you will find samples that attempt to break off specific features and specific usage scenarios into bite sized chunks. For example, the ExecuteIndirect sample will show you just enough about execute indirect to get started with that feature without diving too deep into multiengine whereas the nBodyGravity sample will delve into multiengine without touching on the execute indirect feature etc. By doing this, we hope to make it easier to get started with DirectX 12.

## Visual Studio Templates
In the Templates directory you will find copies of the projects that are available for creation in Visual Studio's "New Project" wizard.

## MiniEngine: A DirectX 12 Engine Starter Kit
In addition to the samples, we are announcing the first DirectX 12 preview release of the MiniEngine.

It came from a desire to quickly dive into graphics and performance experiments.  We knew we would need some basic building blocks whenever starting a new 3D app, and we had already written these things at countless previous gigs.  We got tired of reinventing the wheel, so we established our own core library of helper classes and platform abstractions.  We wanted to be able to create a new app by writing just the Init(), Update(), and Render() functions and leveraging as much reusable code as possible.  Today our core library has been redesigned for DirectX 12 and aims to serve as an example of efficient API usage.  It is obviously not exhaustive of what a game engine needs, but it can serve as the cornerstone of something new.  You can also borrow whatever useful code you find.

### Some features of MiniEngine
* High-quality anti-aliased text rendering
* Real-time CPU and GPU profiling
* User-controlled variables
* Game controller, mouse, and keyboard input
* A friendly DirectXMath wrapper
* Perspective camera supporting traditional and reversed Z matrices
* Asynchronous DDS texture loading and ZLib decompression
* Large library of shaders
* Easy shader embedding via a compile-to-header system
* Easy render target, depth target, and unordered access view creation
* A thread-safe GPU command context system (WIP)
* Easy-to-use dynamic constant buffers and descriptor tables

## Requirements
* Windows 10
* [Visual Studio 2017](https://www.visualstudio.com/) with the [Windows 10 Creator Update SDK](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk)

## Contributing
We're always looking for your help to fix bugs and improve the samples.  File those pull requests and we'll be happy to take a look.

Find more information on DirectX 12 on our blog: http://blogs.msdn.com/b/directx/

Troubleshooting information for this repository can be found in the site [Wiki](https://github.com/Microsoft/DirectX-Graphics-Samples/wiki).

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
