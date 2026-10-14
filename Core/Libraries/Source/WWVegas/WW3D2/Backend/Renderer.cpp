/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// EastWind bridge for upstream's static Renderer interface. Upstream defines these
// methods in its DX8 backend; here they forward into the IRenderBackend instance
// that Renderer::Init creates, which is the Direct3D 12 backend. Every forward is
// a no-op while no backend exists, matching the null checks EastWind's callers
// made before they were migrated to Renderer.

#include "WW3D2/Renderer.h"
#include "WW3D2/IRenderBackend.h"
#include "RenderBackend.h"

#include "WWDebug/wwdebug.h"

static IRenderBackend *Backend = nullptr;

IRenderBackend *Get_Renderer_Backend()
{
    return Backend;
}

bool Renderer::Init(void * window, bool lite)
{
    WWASSERT(Backend == nullptr);
    if (Backend == nullptr)
    {
        Backend = Create_Render_Backend(window, lite);
    }
    return Backend != nullptr;
}

void Renderer::Shutdown()
{
    delete Backend;
    Backend = nullptr;
}

void Renderer::Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit)
{
    if (Backend != nullptr)
        Backend->Set_Gamma(gamma, bright, contrast, calibrate, uselimit);
}

void Renderer::Begin_Scene()
{
    if (Backend != nullptr)
        Backend->Begin_Scene();
}

void Renderer::End_Scene(bool flip_frame)
{
    if (Backend != nullptr)
        Backend->End_Scene(flip_frame);
}

void Renderer::Flip_To_Primary()
{
    if (Backend != nullptr)
        Backend->Flip_To_Primary();
}

void Renderer::Clear(bool clear_color, bool clear_z_stencil,
                     const Vector3 & color,
                     float dest_alpha, float z, unsigned int stencil)
{
    if (Backend != nullptr)
        Backend->Clear(clear_color, clear_z_stencil, color, dest_alpha, z, stencil);
}

void Renderer::Set_Viewport(const RenderViewport & viewport)
{
    if (Backend == nullptr)
        return;

    RenderBackendViewport vp;
    vp.x      = viewport.x;
    vp.y      = viewport.y;
    vp.width  = viewport.width;
    vp.height = viewport.height;
    vp.min_z  = viewport.min_z;
    vp.max_z  = viewport.max_z;
    Backend->Set_Viewport(vp);
}

void Renderer::Invalidate_Cached_Render_States()
{
    if (Backend != nullptr)
        Backend->Invalidate_Cached_Render_States();
}

void Renderer::Set_Ambient(const Vector3 & color)
{
    if (Backend != nullptr)
        Backend->Set_Ambient(color);
}

void Renderer::Set_Light_Environment(LightEnvironmentClass * light_env)
{
    if (Backend != nullptr)
        Backend->Set_Light_Environment(light_env);
}
