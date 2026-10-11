# Keeps JUCE off the GPU on Windows.
#
# JUCE 8 draws windows with Direct2D, makes Direct2D images by default, and creates a
# Direct3D device per graphics adapter. Inside Adobe Audition that crashed the host with a
# stack overflow in d3d11 / the NVIDIA driver whenever a JUCE window opened. With
# JUCE_GHOST_NO_GPU=1 these patches route JUCE through its existing software path (the one
# it already uses under Wine): GDI windows, software images, and DXGI outputs only (for
# vblank timing) with no Direct3D device.
#
# Applied at configure time and idempotent. Fails if a JUCE upgrade changes the patched
# lines, so the patch is never silently skipped.

function(ghost_patch_file path old new)
    file(READ "${path}" contents)
    string(FIND "${contents}" "${new}" already)

    if(already GREATER -1)
        return()
    endif()

    string(FIND "${contents}" "${old}" found)

    if(found EQUAL -1)
        message(FATAL_ERROR "Ghost JUCE patch: expected text not found in ${path}. Update cmake/PatchJuce.cmake for this JUCE version.")
    endif()

    string(REPLACE "${old}" "${new}" contents "${contents}")
    file(WRITE "${path}" "${contents}")
    message(STATUS "Ghost JUCE patch applied: ${path}")
endfunction()

set(ghost_guard "#ifndef JUCE_GHOST_NO_GPU\n #define JUCE_GHOST_NO_GPU 0\n#endif\n")

# 1. Windows use the software (GDI) renderer instead of Direct2D.
ghost_patch_file("${JUCE_SOURCE_DIR}/modules/juce_gui_basics/native/juce_Windowing_windows.cpp"
    "    const auto renderer = juce_isRunningInWine() ? 0 : 1;"
    "    ${ghost_guard}    const auto renderer = (JUCE_GHOST_NO_GPU || juce_isRunningInWine()) ? 0 : 1; // Ghost patch")

# 2. Default (native) images are software images instead of Direct2D bitmaps.
ghost_patch_file("${JUCE_SOURCE_DIR}/modules/juce_graphics/native/juce_Direct2DImage_windows.cpp"
    "ImagePixelData::Ptr NativeImageType::create (Image::PixelFormat format, int width, int height, bool clearImage) const\n{\n    if (! juce_isRunningInWine())"
    "${ghost_guard}ImagePixelData::Ptr NativeImageType::create (Image::PixelFormat format, int width, int height, bool clearImage) const\n{\n    if (! JUCE_GHOST_NO_GPU && ! juce_isRunningInWine()) // Ghost patch")

# 3. Adapters keep their DXGI outputs (vblank timing) but create no Direct3D/Direct2D device.
ghost_patch_file("${JUCE_SOURCE_DIR}/modules/juce_graphics/native/juce_DirectX_windows.cpp"
    "    // This flag adds support for surfaces with a different color channel ordering\n    // than the API default. It is required for compatibility with Direct2D.\n    const auto creationFlags"
    "    ${ghost_guard}    if (JUCE_GHOST_NO_GPU) // Ghost patch\n        return result;\n\n    // This flag adds support for surfaces with a different color channel ordering\n    // than the API default. It is required for compatibility with Direct2D.\n    const auto creationFlags")
