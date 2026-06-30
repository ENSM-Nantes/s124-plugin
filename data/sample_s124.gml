<?xml version="1.0" encoding="UTF-8"?>
<!--
  S-124 Sample Dataset – Navigational Warnings
  IHO S-124 Edition 2.0.0
  This file demonstrates the three geometry types: polygon, point, line.
-->
<S124:DataSet
    xmlns:S124="http://www.iho.int/S124/gml/2.0"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    gml:id="S124_SAMPLE_DATASET_001">

  <gml:boundedBy>
    <gml:Envelope srsName="EPSG:4326">
      <gml:lowerCorner>47.0 -6.0</gml:lowerCorner>
      <gml:upperCorner>51.0 -2.0</gml:upperCorner>
    </gml:Envelope>
  </gml:boundedBy>

  <!-- Warning 1: Exercise area (polygon) -->
  <imemberOf>
    <S124:NavwarnTypeGeneral gml:id="NW.NAVAREA_I.2024.001">
      <S124:warningNumber>1</S124:warningNumber>
      <S124:year>2024</S124:year>
      <S124:nameOfSeries>NAVAREA I</S124:nameOfSeries>
      <S124:warningType>4</S124:warningType>
      <S124:warningTypeDetails>43</S124:warningTypeDetails>
      <S124:publicationTime>2024-03-15T08:00:00Z</S124:publicationTime>
      <S124:header>
        <S124:language>eng</S124:language>
        <S124:text>SUBMARINE EXERCISE AREA ACTIVATED.
AREA BOUNDED BY: 48°N 005°W, 49°N 005°W, 49°N 003°W, 48°N 003°W.
SUBMARINES OPERATING DIVED. VESSELS ADVISED TO KEEP CLEAR.
EXERCISE PERIOD: 15 TO 20 MARCH 2024, 0600–1800Z DAILY.</S124:text>
      </S124:header>
      <S124:geometry>
        <gml:Polygon gml:id="POLY.NW.001">
          <gml:exterior>
            <gml:LinearRing>
              <gml:posList>48.0 -5.0 49.0 -5.0 49.0 -3.0 48.0 -3.0 48.0 -5.0</gml:posList>
            </gml:LinearRing>
          </gml:exterior>
        </gml:Polygon>
      </S124:geometry>
    </S124:NavwarnTypeGeneral>
  </imemberOf>

  <!-- Warning 2: Light buoy off station (point) -->
  <imemberOf>
    <S124:NavwarnTypeGeneral gml:id="NW.NAVAREA_I.2024.002">
      <S124:warningNumber>2</S124:warningNumber>
      <S124:year>2024</S124:year>
      <S124:nameOfSeries>NAVAREA I</S124:nameOfSeries>
      <S124:warningType>2</S124:warningType>
      <S124:warningTypeDetails>12</S124:warningTypeDetails>
      <S124:publicationTime>2024-03-16T10:30:00Z</S124:publicationTime>
      <S124:header>
        <S124:language>eng</S124:language>
        <S124:text>LIGHT BUOY "OUESSANT SW" DRIFTING OFF STATION.
LAST KNOWN POSITION: 48°31.0'N 005°08.0'W.
MARINERS ARE ADVISED TO NAVIGATE WITH CAUTION IN THE AREA.</S124:text>
      </S124:header>
      <S124:geometry>
        <gml:Point gml:id="PT.NW.002">
          <gml:pos>48.517 -5.133</gml:pos>
        </gml:Point>
      </S124:geometry>
    </S124:NavwarnTypeGeneral>
  </imemberOf>

  <!-- Warning 3: Cable laying operation (line) -->
  <imemberOf>
    <S124:NavwarnTypeGeneral gml:id="NW.NAVAREA_I.2024.003">
      <S124:warningNumber>3</S124:warningNumber>
      <S124:year>2024</S124:year>
      <S124:nameOfSeries>NAVAREA I</S124:nameOfSeries>
      <S124:warningType>2</S124:warningType>
      <S124:warningTypeDetails>78</S124:warningTypeDetails>
      <S124:publicationTime>2024-03-17T06:00:00Z</S124:publicationTime>
      <S124:header>
        <S124:language>eng</S124:language>
        <S124:text>CABLE LAYING OPERATIONS IN PROGRESS.
VESSEL "OCEAN BUILDER" LAYING SUBMARINE POWER CABLE.
ROUTE: 50°00'N 004°00'W TO 50°00'N 002°00'W.
SPEED 2 KT. CABLE CLUMP WEIGHT DEPLOYED AFT.
MARINERS REQUESTED TO KEEP CLEAR BY 500 METRES.</S124:text>
      </S124:header>
      <S124:geometry>
        <gml:LineString gml:id="LS.NW.003">
          <gml:posList>50.0 -4.0 50.0 -3.0 50.0 -2.0</gml:posList>
        </gml:LineString>
      </S124:geometry>
    </S124:NavwarnTypeGeneral>
  </imemberOf>

  <!-- Warning 4: Coastal warning via NwPart reference pattern -->
  <imemberOf>
    <S124:NavwarnTypeGeneral gml:id="NW.COASTAL.2024.004">
      <S124:warningNumber>4</S124:warningNumber>
      <S124:year>2024</S124:year>
      <S124:nameOfSeries>NAVAREA I</S124:nameOfSeries>
      <S124:warningType>1</S124:warningType>
      <S124:warningTypeDetails>5</S124:warningTypeDetails>
      <S124:publicationTime>2024-03-18T14:00:00Z</S124:publicationTime>
      <S124:header>
        <S124:language>eng</S124:language>
        <S124:text>WRECK REPORTED.
POSITION: 47°45'N 003°30'W (APPROX).
SHOAL PATCH DEPTH UNKNOWN. VESSELS TO AVOID THE AREA.</S124:text>
      </S124:header>
      <S124:theWarningPart xlink:href="#NWP.004"/>
    </S124:NavwarnTypeGeneral>
  </imemberOf>

  <imemberOf>
    <S124:NwPart gml:id="NWP.004">
      <S124:geometry>
        <gml:Point gml:id="PT.NWP.004">
          <gml:pos>47.75 -3.5</gml:pos>
        </gml:Point>
      </S124:geometry>
    </S124:NwPart>
  </imemberOf>

</S124:DataSet>
