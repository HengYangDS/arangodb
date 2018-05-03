Geo-Spatial Indexes
===========

This is an introduction to ArangoDB's geo-spatial indexes.

ArangoDB features an [Google S2](http://s2geometry.io/) based geospatial index.
We support indexing on a subset of the **GeoJSON** standard (as well as simple
latitude longitude pairs),  for more information see the [GeoJSON
section](#GeoJSON).

AQL's geospatial utility functions are described in [Geo
functions](../../AQL/Functions/Geo.html). Helper functions to easily create
GeoJSON objects are described in [GeoJSON
Constructors](../../AQL/Functions/GeoConstructors.html).


### Using a Geo-Spatial Index

The geo-spatial index supports containment and intersection
queries on a various geometric 2D shapes. You should be mainly using AQL queries
to perform these types of operations. The index can operate in **two different
modi**, depending on if you want to use the GeoJSON  data-format or not. The modi
are mainly toggled by using the `geoJson` field when creating the index.

#### GeoJSON Mode

To create an index in GeoJSON mode execute:

```
collection.ensureIndex({ type: "geo", fields: [ "geometry" ], geoJson:true })
```

This creates the index on all documents and uses _geometry_ as the attributed
field where the value is either a [Geometry
Object](https://tools.ietf.org/html/rfc7946#section-3.1) **or** a _coordinate
array_. The array must contain at least two numeric values with longitude (first
value) and the latitude (second value). This  corresponds to the format
described in [RFC 7946
Position](https://tools.ietf.org/html/rfc7946#section-3.1.1)

All documents, which do not have the attribute path or have a non-conforming
value in it are excluded from the index.

A geo index is implicitly sparse, and there is no way to control its sparsity.
In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.


#### Non-GeoJSON mode

This index mode exlusively supports indexing on coordinate arrays. Values that
contain GeoJSON or other types of data will be ignores.

To create a geo-spatial index on all documents using *latitude* and
*longitude* as separate attribute paths, two paths need to be specified
in the *fields* array:

`collection.ensureIndex({ type: "geo", fields: [ "latitude", "longitude" ] })`

The first field is always defined to be the _latitude_ and the second is the
_longitude_.

Alternatively you can specify only one field:

`collection.ensureIndex({ type: "geo", fields: [ "location" ], geoJson:false })`

Creates a geo-spatial index on all documents using *location* as path to the
coordinates. The value of the attribute has to be an array with at least two
numeric values. The array must contain the latitude (first value) and the
longitude (second value).

All documents, which do not have the attribute path(s) or have a non-conforming
value in it are excluded from the index.

A geo index is implicitly sparse, and there is no way to control its sparsity.
In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

In case that the index was successfully created, an object with the index
details, including the index-identifier, is returned.

### Indexed GeoSpatial Queries

The geo-spatial index supports a varity of AQL queries, which can build with the help
of the [geo utility functions](../../AQL/Functions/Geo.html). There are three specific
geo functions that can be otpimized, provided that they are used correctly:
`GEO_DISTANCE, GEO_CONTAINS, GEO_INTERSECTS`. Additionally there is support to optimize
the older geo functions `DISTANCE`, `NEAR` and `WITHIN` (the last two only if they are
used in their 4 argument version, without *distanceName*).

When in doubt whether your query is beeing properly optimized, 
check the [AQL explain](../../AQL/ExecutionAndPerformance/ExplainingQueries.html) output to check for index usage.

#### Query for Results near Origin (NEAR type query)

A basic example of a query for results near an origin point:

```
FOR x IN geo_collection
  FILTER GEO_DISTANCE([@lng, @lat], x.geometry) <= 100000
  RETURN x._key
```
The first parameter can be a GeoJSON object or a coordinate array in `[longitude, latitude]` ordering.
The second parameter is the documents field on which the index was created. The function
`GEO_DISTANCE` always returns the distance in meters, so will receive results
up until _100km_.


#### Query for Sorted Results near Origin (NEAR type query)

A basic example of a query for results near an origin point, sorted ascending:

```
FOR x IN geo_collection
  SORT GEO_DISTANCE([@lng, @lat], x.geometry) ASC
  LIMIT 1000
  RETURN x._key
```

The first parameter can be a GeoJSON object or a coordinate array in `[longitude, latitude]` ordering.
The second parameter is the documents field on which the index was created.

You may also get results sorted in a descending order:

```
FOR x IN geo_collection
  SORT GEO_DISTANCE([@lng, @lat], x.geometry) DESC
  LIMIT 1000
  RETURN x._key
```

#### Query for Results within Distance

A query which returns documents within a distance of _1km_ until _100km_.
This will return the documents with a GeoJSON value that is located in the specified search annulus.
```
FOR x IN geo_collection
  FILTER GEO_DISTANCE([@lng, @lat], x.geometry) <= 100000
  FILTER GEO_DISTANCE([@lng, @lat], x.geometry) <= 1000
  RETURN x
```

#### Query for Results contained in Polygon

A query which returns documents contained within a GeoJSON Polygon.
```
LET polygon = GEO_POLYGON([[[60,35],[50,5],[75,10],[70,35]]])
FOR x IN geo_collection
  FILTER GEO_CONTAINS(polygon, x.geometry)
  RETURN x
```
The first parameter of `GEO_CONTAINS` must be a polygon other types are not valid. 
The second parameter must contain the document field on which the index was created.


#### Query for Results Intersecting a Polygon

A query which returns documents contained within a GeoJSON Polygon.
```
LET polygon = GEO_POLYGON([[[60,35],[50,5],[75,10],[70,35]]])
FOR x IN geo_collection
  FILTER GEO_INTERSECTS(polygon, x.geometry)
  RETURN x
```
The first parameter of `GEO_CONTAINS` must be a polygon other types are not valid. 
The second parameter must contain the document field on which the index was created.


### GeoJSON

GeoJSON is a geospatial data format based on JSON. It defines several different
types of JSON objects and the way in which they can be combined to represent
data about geographic shapes on the earth surface. GeoJSON uses a geographic
coordinate reference system, World Geodetic System 1984, and units of decimal
degrees.

Internally ArangoDB maps all coordinates onto a unit sphere, distances are
projected onto a sphere with the Earth's *Volumetric mean radius* of *6371
km*. ArangoDB implements a useful subset of the GeoJSON format [(RFC
7946)](https://tools.ietf.org/html/rfc7946). We do not support Feature Objects
or the GeometryCollection type, supported geometry object types are listed
below:

#### Point

The following section of the RFC specifies a [GeoJSON
Point](https://tools.ietf.org/html/rfc7946#section-3.1.2):
```
{
  "type": "Point",
  "coordinates": [100.0, 0.0]
}
```

#### MultiPoint

The following section of the RFC specifies a [GeoJSON
MultiPoint](https://tools.ietf.org/html/rfc7946#section-3.1.7):
```
{
  "type": "MultiPoint",
  "coordinates": [
    [100.0, 0.0],
    [101.0, 1.0]
  ]
}
```


#### LineString

The following section of the RFC specifies a [GeoJSON
LineString](https://tools.ietf.org/html/rfc7946#section-3.1.4):
```
{
  "type": "LineString",
  "coordinates": [
    [100.0, 0.0],
    [101.0, 1.0]
  ]
}
```

#### MultiLineString

The following section of the RFC specifies a [GeoJSON
MultiLineString](https://tools.ietf.org/html/rfc7946#section-3.1.5): The
"coordinates" member is an array of LineString coordinate arrays:
```
{
  "type": "MultiLineString",
  "coordinates": [
    [
      [100.0, 0.0],
      [101.0, 1.0]
    ],
    [
      [102.0, 2.0],
      [103.0, 3.0]
    ]
  ]
}
```

#### Polygon

[GeoJSON polygons](https://tools.ietf.org/html/rfc7946#section-3.1.6) consists
of a series of closed `LineString` objects (ring-like). These *LineRing* objects
consists of four or more vertices with the first and last coordinate pairs
beeing equal. Coordinates of a Polygon are an array of linear ring coordinate
arrays.  The first element in the array represents the exterior ring.  Any
subsequent elements represent interior rings (or holes).

No holes:
```
{
  "type": "Polygon",
    "coordinates": [
    [
      [100.0, 0.0],
      [101.0, 0.0],
      [101.0, 1.0],
      [100.0, 1.0],
      [100.0, 0.0]
    ]
  ]
}
```

With holes:
- The exterior ring should not self-intersect.
- The interior rings must be contained in the outer ring
- Interior rings cannot overlap (or touch) with each other
```
{
  "type": "Polygon",
  "coordinates": [
    [
      [100.0, 0.0],
      [101.0, 0.0],
      [101.0, 1.0],
      [100.0, 1.0],
      [100.0, 0.0]
    ],
    [
      [100.8, 0.8],
      [100.8, 0.2],
      [100.2, 0.2],
      [100.2, 0.8],
      [100.8, 0.8]
    ]
  ]
}
```

#### MultiPolygon

The following section of the RFC specifies a [GeoJSON
MultiPolygon](https://tools.ietf.org/html/rfc7946#section-3.1.7): The
"coordinates" member is an array of Polygon coordinate arrays.
```
{
  "type": "MultiPolygon",
  "coordinates": [
    [[
      [102.0, 2.0],
      [103.0, 2.0],
      [103.0, 3.0],
      [102.0, 3.0],
      [102.0, 2.0]
    ]],
    [[
      [100.0, 0.0],
      [101.0, 0.0],
      [101.0, 1.0],
      [100.0, 1.0],
      [100.0, 0.0]
    ], [
      [100.2, 0.2],
      [100.2, 0.8],
      [100.8, 0.8],
      [100.8, 0.2],
      [100.2, 0.2]
    ]]
  ]
}
```