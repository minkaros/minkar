// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// minkar-vkdraw's triangle: three corners from the vertex index, no buffers.
#version 450

void main()
{
	const vec2 corners[3] = vec2[](vec2(0.0, -0.75), vec2(0.75, 0.75), vec2(-0.75, 0.75));
	gl_Position = vec4(corners[gl_VertexIndex], 0.0, 1.0);
}
