#include "GlSplatRenderer.h"
#include "ShapeRenderer.h"
#include <qglviewer/camera.h>

#include <SurfaceMeshHelper.h>
using namespace SurfaceMesh;

ShapeRenderer::ShapeRenderer(QString filename, QColor color, int resolution) : color(color)
{
    int w = resolution, h = resolution;
    setMinimumSize(w,h);
    setMaximumSize(w,h);

	// Placement off-screen
	int x = -w * 1.2;
	int y = 0;
	this->setGeometry(x,y,w,h);

    QGLFormat f;
    f.setAlpha(true);
    f.setSampleBuffers(true);
    QGLFormat::setDefaultFormat(f);
    this->setFormat(f);

    // Read mesh file
	SurfaceMeshModel mesh;
	mesh.read(qPrintable(filename));
	mesh.update_face_normals();
	mesh.update_vertex_normals();
	mesh.updateBoundingBox();

	Vector3 bbmin = mesh.bbox().min(), bbmax = mesh.bbox().max();
	bmin = QVector3D(bbmin[0],bbmin[1],bbmin[2]);
	bmax = QVector3D(bbmax[0],bbmax[1],bbmax[2]);

	// Save into OpenGL friendly arrays
	Vector3VertexProperty m_points = mesh.vertex_coordinates(); 
	Vector3VertexProperty m_normals = mesh.vertex_normals();

	foreach(Vertex v, mesh.vertices()){
		Vector3 p = m_points[v];
		Vector3 n = m_normals[v];
		vertices.push_back(GLVertex(p[0],p[1],p[2], n[0],n[1],n[2]));
	}

	foreach(Face f, mesh.faces()){
		foreach(Vertex v, mesh.vertices(f))
			indices.push_back(v.idx());
	}
}

void ShapeRenderer::initializeGL()
{
    // Setup lights and material
	GLfloat ambientLightColor[] = {0.2f,0.2f,0.2f,1};
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLightColor);

    GLfloat diffuseLightColor[] = {0.9f,0.9f,0.9f,1};
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLightColor);

	GLfloat specularLightColor[] = {0.95f,0.95f,0.95f,1};
	glLightfv(GL_LIGHT0, GL_SPECULAR, specularLightColor);

	float posLight0[] = { 3, 3, 3, 0 };
	glLightfv(GL_LIGHT0, GL_POSITION, posLight0);

    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    // Specular lighting
	float specReflection[] = { 0.8f, 0.8f, 0.8f, 1.0f };
	glMaterialfv(GL_FRONT, GL_SPECULAR, specReflection);
	glMateriali(GL_FRONT, GL_SHININESS, 56);
}

void ShapeRenderer::setupCamera()
{
	qglviewer::Camera * sceneCamera = new qglviewer::Camera;

	int w = width(), h = height();
	glViewport( 0, 0, w, h );
	sceneCamera->setScreenWidthAndHeight(w,h);

	sceneCamera->setSceneRadius( 10 );
	sceneCamera->showEntireScene();
	sceneCamera->setUpVector(qglviewer::Vec(0,0,1));
	sceneCamera->setPosition(qglviewer::Vec(-2,-2,1.5));
	sceneCamera->lookAt(qglviewer::Vec());
	sceneCamera->setType(qglviewer::Camera::PERSPECTIVE);

	bool isFitToMesh = true;
	if( isFitToMesh )
	{
		qglviewer::Vec viewDir = sceneCamera->viewDirection();
		Eigen::AlignedBox3d bbox(Vector3(bmin.x(),bmin.y(),bmin.z()),Vector3(bmax.x(),bmax.y(),bmax.z()));
		double distance = bbox.diagonal().size() * 0.9;
		Vector3 center = bbox.center();
		Vector3 newPos = center - (distance * Vector3(viewDir[0], viewDir[1], viewDir[2]));

		sceneCamera->setRevolveAroundPoint( qglviewer::Vec(center) );
		qglviewer::Vec new_pos(newPos);
		sceneCamera->frame()->setPositionWithConstraint(new_pos);
	}

	sceneCamera->loadProjectionMatrix();
	sceneCamera->loadModelViewMatrix();
}

void ShapeRenderer::paintGL()
{
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	setupCamera();
	
	bool isTestScene = false;
	if( isTestScene )
	{
		// Draw a white grid "floor" for the tetrahedron to sit on.
		glColor3f(1.0, 1.0, 1.0);
		glBegin(GL_LINES);
		for (GLfloat i = -2.5; i <= 2.5; i += 0.25) {
			glVertex3f(i, 2.5, 0); glVertex3f(i, -2.5, 0);
			glVertex3f(2.5, i, 0); glVertex3f(-2.5, i, 0);
		}
		glEnd();

		// Draw the tetrahedron.
		glBegin(GL_TRIANGLE_STRIP);
		glColor3f(1, 1, 1); glVertex3f(0, 0, 2);
		glColor3f(1, 0, 0); glVertex3f(-1, 1, 0);
		glColor3f(0, 1, 0); glVertex3f(1, 1, 0);
		glColor3f(0, 0, 1); glVertex3f(0, -1.4f, 0);
		glColor3f(1, 1, 1); glVertex3f(0, 0, 2);
		glColor3f(1, 0, 0); glVertex3f(-1, 1, 0);
		glEnd();
	}

	// Lighting and color
	glEnable( GL_LIGHTING );
	qglColor( color );

	// Draw mesh
	GLuint vertexbuffer;
	GLuint elementbuffer;
	{
		// Upload geometry to hardware
		glGenBuffers(1, &vertexbuffer);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLVertex) * vertices.size(), &vertices[0].x, GL_STATIC_DRAW);

		glGenBuffers(1, &elementbuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * indices.size(), &indices[0], GL_STATIC_DRAW);
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	// Bind vertices data
	glBindBuffer( GL_ARRAY_BUFFER, vertexbuffer );
	glVertexPointer(3, GL_FLOAT, sizeof(GLVertex), (void*)offsetof(GLVertex, x));
	glNormalPointer(GL_FLOAT, sizeof(GLVertex), (void*)offsetof(GLVertex, nx));

	// Bind triangles data then draw
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
	glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
}

QPixmap ShapeRenderer::render(QString filename)
{
   ShapeRenderer renderer(filename, QColor(203, 127, 92), 512);
   renderer.show();
   renderer.updateGL();
   return QPixmap::fromImage( renderer.grabFrameBuffer(true) );
}