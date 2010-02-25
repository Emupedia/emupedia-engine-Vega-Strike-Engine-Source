#include "cont_terrain.h"
#include "universe.h"
#include "star_system.h"
#include "gfx/matrix.h"
#include "vegastrike.h"
#include "gfx/mesh.h"
#include "unit_generic.h"
#include "collide2/Stdafx.h"
#include "collide2/CSopcodecollider.h"
#include "collide2/csgeom2/optransfrm.h"
#include "collide2/basecollider.h"

#include "unit_collide.h"
#include "vs_globals.h"
#include "config_xml.h"
ContinuousTerrain::ContinuousTerrain( const char *filename, const Vector &Scales, const float mass )
{
    float tmass;
    FILE *fp = VSFileSystem::vs_open( filename, "r" );
    if (fp) {
        VSFileSystem::vs_fscanf( fp, "%d %f\n<%f %f %f>", &width, &tmass, &this->Scales.i, &this->Scales.j, &this->Scales.k );
        if (mass)
            tmass = mass;
        if (Scales.i && Scales.j && Scales.k) {
            this->Scales.i *= Scales.i;
            this->Scales.j *= Scales.j;
            this->Scales.k *= Scales.k;
        }
        numcontterr = width*width;
        data = new Terrain*[numcontterr];
        md   = new MeshDat[numcontterr];
        std::string *filenames = new std::string[numcontterr];
        int i;
        for (i = 0; i < numcontterr; i++) {
            data[i]        = NULL;
            md[i].mesh     = NULL;
            md[i].collider = NULL;
            char tmp[512];
            VSFileSystem::vs_fscanf( fp, "%511s", tmp );
            tmp[511]       = '\0';
            for (int k = 0; k < 512; k++) {
                if (tmp[k] == '^') {
                    tmp[k]     = '\0';

                    vector< bsp_polygon >polies;
                    md[i].mesh = Mesh::LoadMesh( tmp, Vector( 1, 1, 1 ), 0, NULL );
                    sscanf( tmp+i+1, "%f,%f", &sizeX, &sizeZ );
                    md[i].mesh->GetPolys( polies );
                    sizeX = md[i].mesh->corner_max().i-md[i].mesh->corner_min().i;
                    sizeZ = md[i].mesh->corner_max().k-md[i].mesh->corner_min().k;
                    md[i].collider = new csOPCODECollider( polies );
                }
                if (tmp[k] == '\0')
                    break;
            }
            filenames[i] = tmp;
        }
        VSFileSystem::vs_close( fp );
        for (i = 0; i < width; i++)
            for (int j = 0; j < width; j++) {
                updateparity *up = &identityparity;
                if (i%2 && j%2)
                    up = &sideupparityodd;
                else if (j%2)
                    up = &sideparityodd;
                else if (i%2)
                    up = &upparityodd;
                if (md[i*width+j].mesh == NULL)
                    data[i*width+j] = new Terrain( filenames[i*width+j].c_str(), this->Scales, tmass, 0, up );
            }
        location = new QVector[numcontterr];
        dirty    = new bool[numcontterr];
        delete[] filenames;
        if (data[0]) {
            sizeX = data[0]->getSizeX();
            sizeZ = data[0]->getSizeZ();
        }
        for (i = 0; i < numcontterr; i++)
            if (data[i]) {
                if ( sizeX != data[i]->getSizeX() || sizeZ != data[i]->getSizeZ() )
                    VSFileSystem::vs_fprintf( stderr,
                                              "Warning: Sizes of terrain do not match...expect gaps in continuous terrain\n" );
                data[i]->SetTotalSize( sizeX*width, sizeZ*width );
            }
        for (i = 0; i < width; i++)
            for (int j = 0; j < width; j++) {
                int nj = j-1 < 0 ? width-1 : j-1;
                int ni = i-1 < 0 ? width-1 : i-1;
                if (data[j+width*i] && data[(j+1)%width+width*i] && data[j+width*( (i+1)%width )] && data[nj+width*i]
                    && data[j+width*ni]) {
                    data[j+width*i]->SetNeighbors( data[(j+1)%width+width*i],
                                                   data[j+width*( (i+1)%width )],
                                                   data[nj+width*i],
                                                   data[j+width*ni] );
                    data[j+width*i]->StaticCullData( 25 );
                }
                location[j+width*i].Set( 0+sizeX*j, 0, 0-sizeZ*i );
            }
        Matrix tmpmat;
        Identity( tmpmat );
        SetTransformation( tmpmat );
    } else {
        numcontterr = 0;
        width = 0;
        dirty = NULL;
        location    = NULL;
        data = NULL;
        md = NULL;
    }
}

ContinuousTerrain::~ContinuousTerrain()
{
    for (int i = 0; i < numcontterr; i++) {
        if (data[i])
            delete data[i];
        if (md[i].mesh)
            delete md[i].mesh;
        if (md[i].collider)
            delete md[i].collider;
    }
    if (dirty)
        delete[] dirty;
    if (location)
        delete[] location;
    if (data)
        delete[] data;
    if (md)
        delete[] md;
}
void ContinuousTerrain::Collide()
{
    for (int i = 0; i < numcontterr; i++) {
        if (data[i])
            data[i]->Collide();
        //FIXME
        else
            assert( 0 );
    }
}
void ContinuousTerrain::Collide( Unit *un )
{
    bool datacol = false;
    for (int i = 0; i < numcontterr; i++) {
        if (data[i])
            data[i]->Collide( un );
        else
            datacol = true;
    }
    if (datacol)
        Collide( un, transformation );
}
QVector ContinuousTerrain::GetGroundPosIdentTrans( QVector ShipPos, Vector &norm )
{
    Matrix ident;
    Identity( ident );
    ShipPos.i /= Scales.i;
    ShipPos.j /= Scales.j;
    ShipPos.k /= Scales.k;
    for (int i = 0; i < numcontterr; i++) {
        QVector tmploc = ShipPos-location[i]+QVector( (data[i])->getminX()+.5*(data[i])->getSizeX(), 0,
                                                     (data[i])->getminZ()+.5*(data[i])->getSizeZ() );
        if ( data[i]->GetGroundPos( tmploc, norm, ident, sizeX*width, sizeZ*width ) ) {
            tmploc   += location[i]-QVector( (data[i])->getminX()+.5*(data[i])->getSizeX(), 0,
                                            (data[i])->getminZ()+.5*(data[i])->getSizeZ() );

            tmploc.i *= Scales.i;
            tmploc.j *= Scales.j;
            tmploc.k *= Scales.k;
            return tmploc;
        }
    }
    VSFileSystem::vs_fprintf( stderr, "Can't find %f,%f,%f\n", ShipPos.i, ShipPos.j, ShipPos.k );
    ShipPos.i *= Scales.i;
    ShipPos.j *= Scales.j;
    ShipPos.k *= Scales.k;
    return ShipPos;
}
QVector ContinuousTerrain::GetGroundPos( QVector ShipPos, Vector &norm )
{
    for (int i = 0; i < numcontterr; i++)
        if ( data[i]->GetGroundPos( ShipPos, norm, sizeX*width, sizeZ*width ) )
            return ShipPos;
    return ShipPos;
}
void ContinuousTerrain::DisableDraw()
{
    for (int i = 0; i < numcontterr; i++)
        if (data[i])
            data[i]->DisableDraw();
}
void ContinuousTerrain::DisableUpdate()
{
    for (int i = 0; i < numcontterr; i++)
        if (data[i])
            data[i]->DisableUpdate();
}
void ContinuousTerrain::EnableDraw()
{
    for (int i = 0; i < numcontterr; i++)
        if (data[i])
            data[i]->EnableDraw();
}
void ContinuousTerrain::EnableUpdate()
{
    for (int i = 0; i < numcontterr; i++)
        if (data[i])
            data[i]->EnableUpdate();
}

void ContinuousTerrain::Draw()
{
    for (int i = 0; i < numcontterr; i++) {
        if (data[i]) {
            data[i]->Render();
        } else if (md[i].mesh) {
            Vector TransformedPosition = Transform( md[i].mat,
                                                   md[i].mesh->Position() );
            float  d = GFXSphereInFrustum( TransformedPosition,
                                           md[i].mesh->rSize()
                                         );
            if (d)
                md[i].mesh->Draw( 1000, md[i].mat, d, -1, (_Universe->AccessCamera()->GetNebula() != NULL) ? -1 : 0 );
        }
    }
}
void ContinuousTerrain::SetTransformation( const Matrix &transformation )
{
    CopyMatrix( this->transformation, transformation );
    ScaleMatrix( this->transformation, Scales );
    for (int i = 0; i < numcontterr; i++)
        dirty[i] = true;
    //AdjustTerrain();
}

bool ContinuousTerrain::checkInvScale( double &pos, double campos, float size )
{
    bool  retval = false;
    size *= width;
    float tmp    = pos-campos;
    while ( fabs( tmp-size ) < fabs( tmp ) ) {
        tmp   -= size;
        retval = true;
    }
    while ( fabs( tmp+size ) < fabs( tmp ) ) {
        tmp   += size;
        retval = true;
    }
    if (retval)
        pos = tmp+campos;
    return retval;
}
void ContinuousTerrain::Collide( Unit *un, Matrix t )
{
    Matrix transform;
    if (un->isUnit() == BUILDINGPTR)
        return;
    ScaleMatrix( t, Scales );
    //CopyMatrix (t,transformation);
    CopyMatrix( transform, t );
    for (int i = 0; i < numcontterr; i++) {
        //Vector tmp;
        QVector tmp( Transform( t, location[i]-QVector(
                                   (data[i] ? (data[i])->getminX() : md[i].mesh->corner_min().i)+.5
                                   *( data[i] ? (data[i])->getSizeX() : (md[i].mesh->corner_max().i
                                                                         -md[i].mesh->corner_min().i) ),
                                   0,
                                   (data[i] ? (data[i])->getminZ() : md[i].mesh->corner_min().k)
                                   +( .5
                                     *( data[i] ? (data[i])->getSizeZ() : (md[i].mesh->corner_max().i-md[i].mesh->corner_min(
                                                                                                                            )
                                                                           .i) ) ) ) ) );
        //tmp=Vector (Transform (t,location[i]-Vector (md[i].mesh->corner_min().i+.5*(md[i].mesh->corner_max().i-md[i].mesh->corner_min().i),0,md[i].mesh->corner_min().k+.5*(md[i].mesh->corner_max().k-md[i].mesh->corner_min().k))));
        //}

        transform.p = tmp;
        if (data[i]) {
            data[i]->Collide( un, transform );
        } else {
            //#if 0
            bool    autocol = false;
            QVector diff    = InvScaleTransform( t, un->Position() );
            if (diff.j < 0) autocol = true;
            diff.i = fmod( (double) diff.i, (double) sizeX*width );
            if (diff.i < 0)
                diff.i += sizeX*width;
            diff.k = fmod( (double) diff.k, (double) sizeZ*width );
            if (diff.k < 0)
                diff.k += sizeZ*width;
            if ( !(rand()%10) )
                VSFileSystem::vs_fprintf( stderr, "unit in out sapce %f %f %f\n", diff.i, diff.j, diff.k );
            diff = Transform( t, diff );
            const csReversibleTransform bigtransform( transform );
            Matrix smallmat = ( un->GetTransformation() );

            smallmat.p = diff;
            const csReversibleTransform smalltransform( smallmat );
            //#endif
#if 0
            Matrix transform;
            AdjustTerrain( transform, t, un->Position(), i );
            const csReversibleTransform bigtransform( transform );

            const csReversibleTransform smalltransform( un->GetTransformation() );
#endif
            QVector smallpos, bigpos;
            Vector  smallNormal, bigNormal;
            if (autocol) {
                smallpos    = un->Position();
                bigpos      = un->Position()-QVector( 0, un->rSize(), 0 );
                smallNormal = Vector( 0, -1, 0 );
                bigNormal   = Vector( 0, 1, 0 );
                autocol     = true;
            }
            if (md[i].collider) {
                if (un->colTrees) {
                    if ( un->colTrees->colTree( un, Vector( 0, 0, 0 ) ) ) {
                        if ( un->colTrees->colTree( un, Vector( 0, 0, 0 ) )->Collide( *md[i].collider,
                                                                                      &smalltransform,
                                                                                      &bigtransform ) ) {
                            csCollisionPair *mycollide = csOPCODECollider::GetCollisions();
                            unsigned int     numHits   = csOPCODECollider::GetCollisionPairCount();
                            if (numHits) {
                                smallpos.Set( (mycollide[0].a1.x+mycollide[0].b1.x+mycollide[0].c1.x)/3,
                                             (mycollide[0].a1.y+mycollide[0].b1.y+mycollide[0].c1.y)/3,
                                             (mycollide[0].a1.z+mycollide[0].b1.z+mycollide[0].c1.z)/3 );
                                smallpos = Transform( un->cumulative_transformation_matrix, smallpos );
                                bigpos.Set( (mycollide[0].a2.x+mycollide[0].b2.x+mycollide[0].c2.x)/3,
                                           (mycollide[0].a2.y+mycollide[0].b2.y+mycollide[0].c2.y)/3,
                                           (mycollide[0].a2.z+mycollide[0].b2.z+mycollide[0].c2.z)/3 );
                                bigpos = Transform( transform, bigpos );
                                csVector3 sn, bn;
                                sn.Cross( mycollide[0].b1-mycollide[0].a1, mycollide[0].c1-mycollide[0].a1 );
                                bn.Cross( mycollide[0].b2-mycollide[0].a2, mycollide[0].c2-mycollide[0].a2 );
                                sn.Normalize();
                                bn.Normalize();
                                smallNormal.Set( sn.x, sn.y, sn.z );
                                bigNormal.Set( bn.x, bn.y, bn.z );
                                smallNormal = TransformNormal( un->cumulative_transformation_matrix, smallNormal );
                                bigNormal   = TransformNormal( transform, bigNormal );
                                autocol     = true;
                            }
                        }
                    }
                }
            }
            if (autocol) {
                static float mass = 1000;                 //XMLSupport::parse_float (vs_config->getVariable ("terrain","mass","1000"));
                un->ApplyForce( bigNormal*.4*un->GetMass()*fabs( bigNormal.Dot( (un->GetVelocity()/SIMULATION_ATOM) ) ) );
                un->ApplyDamage( un->Position().Cast()-bigNormal*un->rSize(), -bigNormal, .5
                                 *fabs( bigNormal.Dot( un->GetVelocity() ) )*mass*SIMULATION_ATOM, un, GFXColor( 1,
                                                                                                                 1,
                                                                                                                 1,
                                                                                                                 1 ), NULL );
            }
        }
    }
}

void ContinuousTerrain::AdjustTerrain( Matrix &transform, const Matrix &transformation, const QVector &campos, int i )
{
    dirty[i] |= checkInvScale( location[i].i, campos.i, sizeX );
    dirty[i] |= checkInvScale( location[i].k, campos.k, sizeZ );
    CopyMatrix( transform, transformation );
    QVector tmp( Transform( transformation, location[i]-QVector(
                               (data[i] ? (data[i])->getminX() : md[i].mesh->corner_min().i)+.5
                               *( data[i] ? (data[i])->getSizeX() : (md[i].mesh->corner_max().i-md[i].mesh->corner_min().i) ),
                               0,
                               (data[i] ? (data[i])->getminZ() : md[i].mesh->corner_min().k)
                               +( .5
                                 *( data[i] ? (data[i])->getSizeZ() : (md[i].mesh->corner_max().i-md[i].mesh->corner_min().i) ) ) ) ) );
    transform.p = tmp;
}
void ContinuousTerrain::AdjustTerrain( StarSystem *ss )
{
    Matrix  transform;

    QVector campos = InvScaleTransform( transformation, _Universe->AccessCamera()->GetPosition() );
    for (int i = 0; i < numcontterr; i++)
        if (1 || dirty[i]) {
            AdjustTerrain( transform, transformation, campos, i );
            if (data[i])
                (data[i])->SetTransformation( transform );
            else
                md[i].mat = transform;
            dirty[i] = false;
        }
}

Vector ContinuousTerrain::GetUpVector( const Vector &pos )
{
    return (data[0])
           ? data[0]->GetUpVector( pos )
           : Vector( transformation.getQ() );
}

void disableTerrainDraw( ContinuousTerrain *ct )
{
    ct->DisableDraw();
}

