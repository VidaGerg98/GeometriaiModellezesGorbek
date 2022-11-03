#version 430

layout (location = 0) in vec3 aPos;

out vec3 myColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float pointNum;
uniform float selectedPoint;
uniform float drawingPoints;

void main(void)
{
	gl_Position =  projection * view * model * vec4(aPos, 1.0);

	if (drawingPoints == 1.0) {
		if (gl_VertexID == selectedPoint) {
			myColor = vec3(0.0, 1.0, 0.0);
		}
		else {
			myColor = vec3(1.0, 1.0, 1.0);
		}
	}
	else {
		myColor = vec3(1.0, 1.0, 1.0);
	}
	
}