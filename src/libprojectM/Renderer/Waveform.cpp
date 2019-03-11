/*
 * Waveform.hpp
 *
 *  Created on: Jun 25, 2008
 *      Author: pete
 */

#include "projectM-opengl.h"
#include "Waveform.hpp"
#include <algorithm>
#include <cmath>
#include "BeatDetect.hpp"
#include "ShaderEngine.hpp"
#include <glm/gtc/type_ptr.hpp>

typedef float floatPair[2];

Waveform::Waveform(int _samples)
: RenderItem(),samples(_samples), points(_samples), pointContext(_samples)
{

	spectrum = false; /* spectrum data or pcm data */
	dots = false; /* draw wave as dots or lines */
	thick = false; /* draw thicker lines */
	additive = false; /* add color values together */

	scaling= 1; /* scale factor of waveform */
	smoothing = 0; /* smooth factor of waveform */
	sep = 0;

    Init();
}

void Waveform::InitVertexAttrib() {
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ColoredPoint), (void*)0);    // points
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ColoredPoint), (void*)(sizeof(float)*2));    // colors
}

void Waveform::Draw(RenderContext &context)
 {
    // scale PCM data based on vol_history to make it more or less independent of the application output volume
    const float  vol_scale = context.beatDetect->getPCMScale();

    float *value1 = new float[samples];
	float *value2 = new float[samples];
	context.beatDetect->pcm->getPCM( value1, samples, 0, spectrum, smoothing, 0);
	context.beatDetect->pcm->getPCM( value2, samples, 1, spectrum, smoothing, 0);

	float mult= scaling*( spectrum ? 0.015f :1.0f);

		std::transform(&value1[0],&value1[samples],&value1[0],std::bind2nd(std::multiplies<float>(),mult));
		std::transform(&value2[0],&value2[samples],&value2[0],std::bind2nd(std::multiplies<float>(),mult));

	WaveformContext waveContext(samples, context.beatDetect);

	for(int x=0;x< samples;x++)
	{
		waveContext.sample = x/(float)(samples - 1);
		waveContext.sample_int = x;
		waveContext.left  = vol_scale * value1[x];
		waveContext.right = vol_scale * value2[x];

		points[x] = PerPoint(points[x],waveContext);
	}

    std::vector<ColoredPoint> points_transf = points;

    for (std::vector<ColoredPoint>::iterator iter = points_transf.begin(); iter != points_transf.end(); ++iter) {
        (*iter).y = -( (*iter).y-1);
        (*iter).a *= masterAlpha;
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vboID);

    glBufferData(GL_ARRAY_BUFFER, sizeof(ColoredPoint) * samples, NULL, GL_DYNAMIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ColoredPoint) * samples, &points_transf[0], GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUseProgram(context.programID_v2f_c4f);

    glUniformMatrix4fv(context.uniform_v2f_c4f_vertex_tranformation, 1, GL_FALSE, glm::value_ptr(context.mat_ortho));

	if (additive)  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (thick)
	{
		glLineWidth(context.texsize <= 512 ? 2 : 2*context.texsize/512);

#ifndef GL_TRANSITION
		glPointSize(context.texsize <= 512 ? 2 : 2*context.texsize/512);
#endif
        glUniform1f(context.uniform_v2f_c4f_vertex_point_size, context.texsize <= 512 ? 2 : 2*context.texsize/512);
	}
    else
    {
#ifndef GL_TRANSITION
        glPointSize(context.texsize <= 512 ? 1 : context.texsize/512);
#endif
        glUniform1f(context.uniform_v2f_c4f_vertex_point_size, context.texsize <= 512 ? 1 : context.texsize/512);
    }

    glBindVertexArray(m_vaoID);

    if (dots)	glDrawArrays(GL_POINTS,0,samples);
    else  	glDrawArrays(GL_LINE_STRIP,0,samples);

    glBindVertexArray(0);

	glLineWidth(context.texsize < 512 ? 1 : context.texsize/512);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	delete[] value1;
	delete[] value2;
   }
