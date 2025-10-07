#include "MeshLoader.h"
#include "STLParser.h"

#ifdef USE_ASSIMP
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#endif

#include <QObject>

MeshLoader::MeshLoader() = default;

MeshBuffer MeshLoader::load(const QString &path, QString *errorMessage) const
{
#ifdef USE_ASSIMP
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path.toStdString(),
                                            aiProcess_Triangulate |
                                                aiProcess_JoinIdenticalVertices |
                                                aiProcess_GenNormals |
                                                aiProcess_ImproveCacheLocality |
                                                aiProcess_OptimizeMeshes);
    if (scene && scene->HasMeshes()) {
        MeshBuffer buffer;
        bool allHaveNormals = true;
        for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
            const aiMesh *mesh = scene->mMeshes[meshIndex];
            if (!mesh)
                continue;
            const unsigned int baseIndex = buffer.positions.size();
            for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
                const aiVector3D &v = mesh->mVertices[i];
                buffer.positions.append(QVector3D(v.x, v.y, v.z));
                if (mesh->HasNormals()) {
                    const aiVector3D &n = mesh->mNormals[i];
                    buffer.normals.append(QVector3D(n.x, n.y, n.z));
                }
            }
            if (!mesh->HasNormals())
                allHaveNormals = false;

            for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                const aiFace &face = mesh->mFaces[f];
                if (face.mNumIndices < 3)
                    continue;
                for (unsigned int idx = 0; idx < 3; ++idx)
                    buffer.indices.append(baseIndex + face.mIndices[idx]);
            }
        }
        if (!allHaveNormals)
            buffer.normals.clear();
        buffer.hasNormals = allHaveNormals && !buffer.normals.isEmpty();
        if (!buffer.positions.isEmpty())
            return buffer;
        if (errorMessage)
            *errorMessage = QObject::tr("Assimp imported scene without vertices.");
    } else {
        if (errorMessage)
            *errorMessage = QObject::tr("Assimp error: %1").arg(QString::fromLocal8Bit(importer.GetErrorString()));
    }
#endif

    STLParser parser;
    return parser.parse(path, errorMessage);
}
