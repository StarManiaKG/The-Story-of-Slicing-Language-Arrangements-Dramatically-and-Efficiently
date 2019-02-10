
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2019 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         http://slade.mancubus.net
// Filename:    MapSpecials.cpp
// Description: Various functions for processing map specials and scripts,
//              mostly for visual effects (transparency, colours, slopes, etc.)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
//
// Includes
//
// -----------------------------------------------------------------------------
#include "Main.h"
#include "MapSpecials.h"
#include "Game/Configuration.h"
#include "SLADEMap.h"
#include "Utility/MathStuff.h"
#include "Utility/Tokenizer.h"

using SurfaceType = MapSector::SurfaceType;


// -----------------------------------------------------------------------------
//
// Variables
//
// -----------------------------------------------------------------------------
namespace
{
const double TAU = MathStuff::PI * 2; // Number of radians in the unit circle
} // namespace


// -----------------------------------------------------------------------------
//
// MapSpecials Class Functions
//
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Clear out all internal state
// -----------------------------------------------------------------------------
void MapSpecials::reset()
{
	sector_colours_.clear();
	sector_fadecolours_.clear();
}

// -----------------------------------------------------------------------------
// Process map specials, depending on the current game/port
// -----------------------------------------------------------------------------
void MapSpecials::processMapSpecials(SLADEMap* map) const
{
	// ZDoom
	if (Game::configuration().currentPort() == "zdoom")
		processZDoomMapSpecials(map);
	// Eternity, currently no need for processEternityMapSpecials
	else if (Game::configuration().currentPort() == "eternity")
		processEternitySlopes(map);
}

// -----------------------------------------------------------------------------
// Process a line's special, depending on the current game/port
// -----------------------------------------------------------------------------
void MapSpecials::processLineSpecial(MapLine* line) const
{
	if (Game::configuration().currentPort() == "zdoom")
		processZDoomLineSpecial(line);
}

// -----------------------------------------------------------------------------
// Sets [colour] to the parsed colour for [tag].
// Returns true if the tag has a colour, false otherwise
// -----------------------------------------------------------------------------
bool MapSpecials::tagColour(int tag, ColRGBA* colour)
{
	unsigned a;
	// scripts
	for (a = 0; a < sector_colours_.size(); a++)
	{
		if (sector_colours_[a].tag == tag)
		{
			colour->r = sector_colours_[a].colour.r;
			colour->g = sector_colours_[a].colour.g;
			colour->b = sector_colours_[a].colour.b;
			colour->a = 255;
			return true;
		}
	}

	return false;
}

// -----------------------------------------------------------------------------
// Sets [colour] to the parsed fade colour for [tag].
// Returns true if the tag has a colour, false otherwise
// -----------------------------------------------------------------------------
bool MapSpecials::tagFadeColour(int tag, ColRGBA* colour)
{
	unsigned a;
	// scripts
	for (a = 0; a < sector_fadecolours_.size(); a++)
	{
		if (sector_fadecolours_[a].tag == tag)
		{
			colour->r = sector_fadecolours_[a].colour.r;
			colour->g = sector_fadecolours_[a].colour.g;
			colour->b = sector_fadecolours_[a].colour.b;
			colour->a = 0;
			return true;
		}
	}

	return false;
}

// -----------------------------------------------------------------------------
// Returns true if any sector tags should be coloured
// -----------------------------------------------------------------------------
bool MapSpecials::tagColoursSet() const
{
	return !(sector_colours_.empty());
}

// -----------------------------------------------------------------------------
// Returns true if any sector tags should be coloured by fog
// -----------------------------------------------------------------------------
bool MapSpecials::tagFadeColoursSet() const
{
	return !(sector_fadecolours_.empty());
}

// -----------------------------------------------------------------------------
// Modify sector with [tag]
// -----------------------------------------------------------------------------
void MapSpecials::setModified(SLADEMap* map, int tag) const
{
	for (auto& sector : map->sectors().allWithId(tag))
		sector->setModified();
}

// -----------------------------------------------------------------------------
// Updates any sectors with tags that are affected by any processed
// specials/scripts
// -----------------------------------------------------------------------------
void MapSpecials::updateTaggedSectors(SLADEMap* map)
{
	// scripts
	unsigned a;
	for (a = 0; a < sector_colours_.size(); a++)
		setModified(map, sector_colours_[a].tag);

	for (a = 0; a < sector_fadecolours_.size(); a++)
		setModified(map, sector_fadecolours_[a].tag);
}

// -----------------------------------------------------------------------------
// Process ZDoom map specials, mostly to convert hexen specials to UDMF
// counterparts
// -----------------------------------------------------------------------------
void MapSpecials::processZDoomMapSpecials(SLADEMap* map) const
{
	// Line specials
	for (unsigned a = 0; a < map->nLines(); a++)
		processZDoomLineSpecial(map->line(a));

	// All slope specials, which must be done in a particular order
	processZDoomSlopes(map);
}

// -----------------------------------------------------------------------------
// Process ZDoom line special
// -----------------------------------------------------------------------------
void MapSpecials::processZDoomLineSpecial(MapLine* line) const
{
	// Get special
	int special = line->special();
	if (special == 0)
		return;

	// Get parent map
	auto map = line->parentMap();

	// Get args
	int args[5];
	for (unsigned arg = 0; arg < 5; arg++)
		args[arg] = line->arg(arg);

	// --- TranslucentLine ---
	if (special == 208)
	{
		// Get tagged lines
		vector<MapLine*> tagged;
		if (args[0] > 0)
			map->lines().putAllWithId(args[0], tagged);
		else
			tagged.push_back(line);

		// Get args
		double   alpha = (double)args[1] / 255.0;
		wxString type  = (args[2] == 0) ? "translucent" : "add";

		// Set transparency
		for (auto& l : tagged)
		{
			l->setFloatProperty("alpha", alpha);
			l->setStringProperty("renderstyle", type);

			Log::info(
				3, wxString::Format("Line %d translucent: (%d) %1.2f, %s", l->index(), args[1], alpha, CHR(type)));
		}
	}
}

// -----------------------------------------------------------------------------
// Process 'OPEN' ACS scripts for various specials - sector colours, slopes, etc
// -----------------------------------------------------------------------------
void MapSpecials::processACSScripts(ArchiveEntry* entry)
{
	sector_colours_.clear();
	sector_fadecolours_.clear();

	if (!entry || entry->size() == 0)
		return;

	Tokenizer tz;
	tz.setSpecialCharacters(";,:|={}/()");
	tz.openMem(entry->data(), "ACS Scripts");

	while (!tz.atEnd())
	{
		if (tz.checkNC("script"))
		{
			Log::info(3, "script found");

			tz.adv(2); // Skip script #

			// Check for open script
			if (tz.checkNC("OPEN"))
			{
				Log::info(3, "script is OPEN");

				// Skip to opening brace
				while (!tz.check("{"))
					tz.adv();

				// Parse script
				while (!tz.checkOrEnd("}"))
				{
					// --- Sector_SetColor ---
					if (tz.checkNC("Sector_SetColor"))
					{
						// Get parameters
						auto parameters = tz.getTokensUntil(")");

						// Parse parameters
						long val;
						int  tag = -1;
						int  r   = -1;
						int  g   = -1;
						int  b   = -1;
						for (auto& parameter : parameters)
						{
							if (parameter.text.ToLong(&val))
							{
								if (tag < 0)
									tag = val;
								else if (r < 0)
									r = val;
								else if (g < 0)
									g = val;
								else if (b < 0)
									b = val;
							}
						}

						// Check everything is set
						if (b < 0)
						{
							Log::info(2, "Invalid Sector_SetColor parameters");
						}
						else
						{
							SectorColour sc;
							sc.tag = tag;
							sc.colour.set(r, g, b, 255);
							Log::info(3, wxString::Format("Sector tag %d, colour %d,%d,%d", tag, r, g, b));
							sector_colours_.push_back(sc);
						}
					}
					// --- Sector_SetFade ---
					else if (tz.checkNC("Sector_SetFade"))
					{
						// Get parameters
						auto parameters = tz.getTokensUntil(")");

						// Parse parameters
						long val;
						int  tag = -1;
						int  r   = -1;
						int  g   = -1;
						int  b   = -1;
						for (auto& parameter : parameters)
						{
							if (parameter.text.ToLong(&val))
							{
								if (tag < 0)
									tag = val;
								else if (r < 0)
									r = val;
								else if (g < 0)
									g = val;
								else if (b < 0)
									b = val;
							}
						}

						// Check everything is set
						if (b < 0)
						{
							Log::info(2, "Invalid Sector_SetFade parameters");
						}
						else
						{
							SectorColour sc;
							sc.tag = tag;
							sc.colour.set(r, g, b, 0);
							Log::info(3, wxString::Format("Sector tag %d, fade colour %d,%d,%d", tag, r, g, b));
							sector_fadecolours_.push_back(sc);
						}
					}

					tz.adv();
				}
			}
		}

		tz.adv();
	}
}

// -----------------------------------------------------------------------------
// Process ZDoom slope specials
// -----------------------------------------------------------------------------
void MapSpecials::processZDoomSlopes(SLADEMap* map) const
{
	// ZDoom has a variety of slope mechanisms, which must be evaluated in a
	// specific order.
	//  - Plane_Align, in line order
	//  - line slope + sector tilt + vavoom, in thing order
	//  - slope copy things, in thing order
	//  - overwrite vertex heights with vertex height things
	//  - vertex triangle slopes, in sector order
	//  - Plane_Copy, in line order

	// First things first: reset every sector to flat planes
	for (unsigned a = 0; a < map->nSectors(); a++)
	{
		auto target = map->sector(a);
		target->setPlane<SurfaceType::Floor>(Plane::flat(target->planeHeight<SurfaceType::Floor>()));
		target->setPlane<SurfaceType::Ceiling>(Plane::flat(target->planeHeight<SurfaceType::Ceiling>()));
	}

	// Plane_Align (line special 181)
	for (unsigned a = 0; a < map->nLines(); a++)
	{
		auto line = map->line(a);
		if (line->special() != 181)
			continue;

		auto sector1 = line->frontSector();
		auto sector2 = line->backSector();
		if (!sector1 || !sector2)
		{
			Log::warning(wxString::Format("Ignoring Plane_Align on one-sided line %d", line->index()));
			continue;
		}
		if (sector1 == sector2)
		{
			Log::warning(wxString::Format(
				"Ignoring Plane_Align on line %d, which has the same sector on both sides", line->index()));
			continue;
		}

		int floor_arg = line->arg(0);
		if (floor_arg == 1)
			applyPlaneAlign<SurfaceType::Floor>(line, sector1, sector2);
		else if (floor_arg == 2)
			applyPlaneAlign<SurfaceType::Floor>(line, sector2, sector1);

		int ceiling_arg = line->arg(1);
		if (ceiling_arg == 1)
			applyPlaneAlign<SurfaceType::Ceiling>(line, sector1, sector2);
		else if (ceiling_arg == 2)
			applyPlaneAlign<SurfaceType::Ceiling>(line, sector2, sector1);
	}

	// Line slope things (9500/9501), sector tilt things (9502/9503), and
	// vavoom things (1500/1501), all in the same pass
	for (unsigned a = 0; a < map->nThings(); a++)
	{
		auto thing = map->thing(a);

		// Line slope things
		if (thing->type() == 9500)
			applyLineSlopeThing<SurfaceType::Floor>(map, thing);
		else if (thing->type() == 9501)
			applyLineSlopeThing<SurfaceType::Ceiling>(map, thing);
		// Sector tilt things
		else if (thing->type() == 9502)
			applySectorTiltThing<SurfaceType::Floor>(map, thing);
		else if (thing->type() == 9503)
			applySectorTiltThing<SurfaceType::Ceiling>(map, thing);
		// Vavoom things
		else if (thing->type() == 1500)
			applyVavoomSlopeThing<SurfaceType::Floor>(map, thing);
		else if (thing->type() == 1501)
			applyVavoomSlopeThing<SurfaceType::Ceiling>(map, thing);
	}

	// Slope copy things (9510/9511)
	for (unsigned a = 0; a < map->nThings(); a++)
	{
		auto thing = map->thing(a);

		if (thing->type() == 9510 || thing->type() == 9511)
		{
			auto target = map->sectors().atPos(thing->position());
			if (!target)
				continue;

			// First argument is the tag of a sector whose slope should be copied
			int tag = thing->arg(0);
			if (!tag)
			{
				Log::warning(
					wxString::Format("Ignoring slope copy thing in sector %d with no argument", target->index()));
				continue;
			}

			auto tagged_sector = map->sectors().firstWithId(tag);
			if (!tagged_sector)
			{
				Log::warning(wxString::Format(
					"Ignoring slope copy thing in sector %d; no sectors have target tag %d", target->index(), tag));
				continue;
			}

			if (thing->type() == 9510)
				target->setFloorPlane(tagged_sector->floor().plane);
			else
				target->setCeilingPlane(tagged_sector->ceiling().plane);
		}
	}

	// Vertex height things
	// These only affect the calculation of slopes and shouldn't be stored in
	// the map data proper, so instead of actually changing vertex properties,
	// we store them in a hashmap.
	VertexHeightMap vertex_floor_heights;
	VertexHeightMap vertex_ceiling_heights;
	for (unsigned a = 0; a < map->nThings(); a++)
	{
		auto thing = map->thing(a);
		if (thing->type() == 1504 || thing->type() == 1505)
		{
			// TODO there could be more than one vertex at this point
			auto vertex = map->vertices().vertexAt(thing->xPos(), thing->yPos());
			if (vertex)
			{
				if (thing->type() == 1504)
					vertex_floor_heights[vertex] = thing->zPos();
				else if (thing->type() == 1505)
					vertex_ceiling_heights[vertex] = thing->zPos();
			}
		}
	}

	// Vertex heights -- only applies for sectors with exactly three vertices.
	// Heights may be set by UDMF properties, or by a vertex height thing
	// placed exactly on the vertex (which takes priority over the prop).
	vector<MapVertex*> vertices;
	for (unsigned a = 0; a < map->nSectors(); a++)
	{
		auto target = map->sector(a);
		vertices.clear();
		target->putVertices(vertices);
		if (vertices.size() != 3)
			continue;

		applyVertexHeightSlope<SurfaceType::Floor>(target, vertices, vertex_floor_heights);
		applyVertexHeightSlope<SurfaceType::Ceiling>(target, vertices, vertex_ceiling_heights);
	}

	// Plane_Copy
	for (unsigned a = 0; a < map->nLines(); a++)
	{
		auto line = map->line(a);
		if (line->special() != 118)
			continue;

		int  tag;
		auto front = line->frontSector();
		auto back  = line->backSector();
		if ((tag = line->arg(0)) && front)
		{
			if (auto sector = map->sectors().firstWithId(tag))
				front->setFloorPlane(sector->floor().plane);
		}
		if ((tag = line->arg(1)) && front)
		{
			if (auto sector = map->sectors().firstWithId(tag))
				front->setCeilingPlane(sector->ceiling().plane);
		}
		if ((tag = line->arg(2)) && back)
		{
			if (auto sector = map->sectors().firstWithId(tag))
				front->setFloorPlane(sector->floor().plane);
		}
		if ((tag = line->arg(3)) && back)
		{
			if (auto sector = map->sectors().firstWithId(tag))
				front->setCeilingPlane(sector->ceiling().plane);
		}

		// The fifth "share" argument copies from one side of the line to the
		// other
		if (front && back)
		{
			int share = line->arg(4);

			if ((share & 3) == 1)
				back->setFloorPlane(front->floor().plane);
			else if ((share & 3) == 2)
				front->setFloorPlane(back->floor().plane);

			if ((share & 12) == 4)
				back->setCeilingPlane(front->ceiling().plane);
			else if ((share & 12) == 8)
				front->setCeilingPlane(back->ceiling().plane);
		}
	}
}

// -----------------------------------------------------------------------------
// Process Eternity slope specials
// -----------------------------------------------------------------------------
void MapSpecials::processEternitySlopes(SLADEMap* map) const
{
	// Eternity plans on having a few slope mechanisms,
	// which must be evaluated in a specific order.
	//  - Plane_Align, in line order
	//  - vertex triangle slopes, in sector order (wip)
	//  - Plane_Copy, in line order

	// First things first: reset every sector to flat planes
	for (unsigned a = 0; a < map->nSectors(); a++)
	{
		auto target = map->sector(a);
		target->setPlane<SurfaceType::Floor>(Plane::flat(target->planeHeight<SurfaceType::Floor>()));
		target->setPlane<SurfaceType::Ceiling>(Plane::flat(target->planeHeight<SurfaceType::Ceiling>()));
	}

	// Plane_Align (line special 181)
	for (unsigned a = 0; a < map->nLines(); a++)
	{
		auto line = map->line(a);
		if (line->special() != 181)
			continue;

		auto sector1 = line->frontSector();
		auto sector2 = line->backSector();
		if (!sector1 || !sector2)
		{
			Log::warning(wxString::Format("Ignoring Plane_Align on one-sided line %d", line->index()));
			continue;
		}
		if (sector1 == sector2)
		{
			Log::warning(wxString::Format(
				"Ignoring Plane_Align on line %d, which has the same sector on both sides", line->index()));
			continue;
		}

		int floor_arg = line->arg(0);
		if (floor_arg == 1)
			applyPlaneAlign<SurfaceType::Floor>(line, sector1, sector2);
		else if (floor_arg == 2)
			applyPlaneAlign<SurfaceType::Floor>(line, sector2, sector1);

		int ceiling_arg = line->arg(1);
		if (ceiling_arg == 1)
			applyPlaneAlign<SurfaceType::Ceiling>(line, sector1, sector2);
		else if (ceiling_arg == 2)
			applyPlaneAlign<SurfaceType::Ceiling>(line, sector2, sector1);
	}

	// Plane_Copy
	vector<MapSector*> sectors;
	for (unsigned a = 0; a < map->nLines(); a++)
	{
		auto line = map->line(a);
		if (line->special() != 118)
			continue;

		int  tag;
		auto front = line->frontSector();
		auto back  = line->backSector();
		if ((tag = line->arg(0)) && front)
		{
			if (auto sector = map->sectors().firstWithId(tag))
				front->setFloorPlane(sector->floor().plane);
		}
		if ((tag = line->arg(1)) && front)
		{
			if (auto sector = map->sectors().firstWithId(tag))
				front->setCeilingPlane(sector->ceiling().plane);
		}
		if ((tag = line->arg(2)) && back)
		{
			if (auto sector = map->sectors().firstWithId(tag))
				front->setFloorPlane(sector->floor().plane);
		}
		if ((tag = line->arg(3)) && back)
		{
			if (auto sector = map->sectors().firstWithId(tag))
				front->setCeilingPlane(sector->ceiling().plane);
		}

		// The fifth "share" argument copies from one side of the line to the
		// other
		if (front && back)
		{
			int share = line->arg(4);

			if ((share & 3) == 1)
				back->setFloorPlane(front->floor().plane);
			else if ((share & 3) == 2)
				front->setFloorPlane(back->floor().plane);

			if ((share & 12) == 4)
				back->setCeilingPlane(front->ceiling().plane);
			else if ((share & 12) == 8)
				front->setCeilingPlane(back->ceiling().plane);
		}
	}
}


// -----------------------------------------------------------------------------
// Applies a Plane_Align special on [line], to [target] from [model]
// -----------------------------------------------------------------------------
template<SurfaceType T> void MapSpecials::applyPlaneAlign(MapLine* line, MapSector* target, MapSector* model) const
{
	vector<MapVertex*> vertices;
	target->putVertices(vertices);

	// The slope is between the line with Plane_Align, and the point in the
	// sector furthest away from it, which can only be at a vertex
	double     this_dist;
	MapVertex* this_vertex;
	double     furthest_dist   = 0.0;
	MapVertex* furthest_vertex = nullptr;
	for (auto& vertex : vertices)
	{
		this_vertex = vertex;
		this_dist   = line->distanceTo(this_vertex->position());
		if (this_dist > furthest_dist)
		{
			furthest_dist   = this_dist;
			furthest_vertex = this_vertex;
		}
	}

	if (!furthest_vertex || furthest_dist < 0.01)
	{
		Log::warning(wxString::Format(
			"Ignoring Plane_Align on line %d; sector %d has no appropriate reference vertex",
			line->index(),
			target->index()));
		return;
	}

	// Calculate slope plane from our three points: this line's endpoints
	// (at the model sector's height) and the found vertex (at this sector's height).
	double modelz  = model->planeHeight<T>();
	double targetz = target->planeHeight<T>();
	Vec3d  p1(line->x1(), line->y1(), modelz);
	Vec3d  p2(line->x2(), line->y2(), modelz);
	Vec3d  p3(furthest_vertex->position(), targetz);
	target->setPlane<T>(MathStuff::planeFromTriangle(p1, p2, p3));
}

// -----------------------------------------------------------------------------
// Applies a line slope special on [thing], to its containing sector in [map]
// -----------------------------------------------------------------------------
template<SurfaceType T> void MapSpecials::applyLineSlopeThing(SLADEMap* map, MapThing* thing) const
{
	int lineid = thing->arg(0);
	if (!lineid)
	{
		Log::warning(wxString::Format("Ignoring line slope thing %d with no lineid argument", thing->index()));
		return;
	}

	// These are computed on first use, to avoid extra work if no lines match
	MapSector* containing_sector = nullptr;
	double     thingz            = 0.;

	for (auto& line : map->lines().allWithId(lineid))
	{
		// Line slope things only affect the sector on the side of the line
		// that faces the thing
		double     side   = MathStuff::lineSide(thing->position(), line->seg());
		MapSector* target = nullptr;
		if (side < 0)
			target = line->backSector();
		else if (side > 0)
			target = line->frontSector();
		if (!target)
			continue;

		// Need to know the containing sector's height to find the thing's true height
		if (!containing_sector)
		{
			containing_sector = map->sectors().atPos(thing->position());
			if (!containing_sector)
				return;
			thingz = containing_sector->plane<T>().heightAt(thing->position()) + thing->zPos();
		}

		// Three points: endpoints of the line, and the thing itself
		auto  target_plane = target->plane<T>();
		Vec3d p1(line->x1(), line->y1(), target_plane.heightAt(line->start()));
		Vec3d p2(line->x2(), line->y2(), target_plane.heightAt(line->end()));
		Vec3d p3(thing->xPos(), thing->yPos(), thingz);
		target->setPlane<T>(MathStuff::planeFromTriangle(p1, p2, p3));
	}
}

// -----------------------------------------------------------------------------
// Applies a tilt slope special on [thing], to its containing sector in [map]
// -----------------------------------------------------------------------------
template<SurfaceType T> void MapSpecials::applySectorTiltThing(SLADEMap* map, MapThing* thing) const
{
	// TODO should this apply to /all/ sectors at this point, in the case of an
	// intersection?
	auto target = map->sectors().atPos(thing->position());
	if (!target)
		return;

	// First argument is the tilt angle, but starting with 0 as straight down;
	// subtracting 90 fixes that.
	int raw_angle = thing->arg(0);
	if (raw_angle == 0 || raw_angle == 180)
		// Exact vertical tilt is nonsense
		return;

	double angle = thing->angle() / 360.0 * TAU;
	double tilt  = (raw_angle - 90) / 360.0 * TAU;
	// Resulting plane goes through the position of the thing
	double z = target->planeHeight<T>() + thing->zPos();
	Vec3d  point(thing->xPos(), thing->yPos(), z);

	double cos_angle = cos(angle);
	double sin_angle = sin(angle);
	double cos_tilt  = cos(tilt);
	double sin_tilt  = sin(tilt);
	// Need to convert these angles into vectors on the plane, so we can take a
	// normal.
	// For the first: we know that the line perpendicular to the direction the
	// thing faces lies "flat", because this is the axis the tilt thing rotates
	// around.  "Rotate" the angle a quarter turn to get this vector -- switch
	// x and y, and negate one.
	Vec3d vec1(-sin_angle, cos_angle, 0.0);

	// For the second: the tilt angle makes a triangle between the floor plane
	// and the z axis.  sin gives us the distance along the z-axis, but cos
	// only gives us the distance away /from/ the z-axis.  Break that into x
	// and y by multiplying by cos and sin of the thing's facing angle.
	Vec3d vec2(cos_tilt * cos_angle, cos_tilt * sin_angle, sin_tilt);

	target->setPlane<T>(MathStuff::planeFromTriangle(point, point + vec1, point + vec2));
}

// -----------------------------------------------------------------------------
// Applies a vavoom slope special on [thing], to its containing sector in [map]
// -----------------------------------------------------------------------------
template<SurfaceType T> void MapSpecials::applyVavoomSlopeThing(SLADEMap* map, MapThing* thing) const
{
	auto target = map->sectors().atPos(thing->position());
	if (!target)
		return;

	int              tid = thing->id();
	vector<MapLine*> lines;
	target->putLines(lines);

	// TODO unclear if this is the same order that ZDoom would go through the
	// lines, which matters if two lines have the same first arg
	for (unsigned a = 0; a < lines.size(); a++)
	{
		if (tid != lines[a]->arg(0))
			continue;

		// Vavoom things use the plane defined by the thing and its two
		// endpoints, based on the sector's original (flat) plane and treating
		// the thing's height as absolute
		if (MathStuff::distanceToLineFast(thing->position(), lines[a]->seg()) == 0)
		{
			Log::warning(wxString::Format("Vavoom thing %d lies directly on its target line %d", thing->index(), a));
			return;
		}

		short height = target->planeHeight<T>();
		Vec3d p1(thing->xPos(), thing->yPos(), thing->zPos());
		Vec3d p2(lines[a]->x1(), lines[a]->y1(), height);
		Vec3d p3(lines[a]->x2(), lines[a]->y2(), height);

		target->setPlane<T>(MathStuff::planeFromTriangle(p1, p2, p3));
		return;
	}

	Log::warning(wxString::Format("Vavoom thing %d has no matching line with first arg %d", thing->index(), tid));
}

// -----------------------------------------------------------------------------
// Returns the floor/ceiling height of [vertex] in [sector]
// -----------------------------------------------------------------------------
template<SurfaceType T> double MapSpecials::vertexHeight(MapVertex* vertex, MapSector* sector) const
{
	// Return vertex height if set via UDMF property
	wxString prop = (T == SurfaceType::Floor ? "zfloor" : "zceiling");
	if (vertex->hasProp(prop))
		return vertex->floatProperty(prop);

	// Otherwise just return sector height
	return sector->planeHeight<T>();
}

// -----------------------------------------------------------------------------
// Applies a slope to sector [target] based on the heights of its vertices
// (triangular sectors only)
// -----------------------------------------------------------------------------
template<SurfaceType T>
void MapSpecials::applyVertexHeightSlope(MapSector* target, vector<MapVertex*>& vertices, VertexHeightMap& heights)
	const
{
	double z1 = heights.count(vertices[0]) ? heights[vertices[0]] : vertexHeight<T>(vertices[0], target);
	double z2 = heights.count(vertices[1]) ? heights[vertices[1]] : vertexHeight<T>(vertices[1], target);
	double z3 = heights.count(vertices[2]) ? heights[vertices[2]] : vertexHeight<T>(vertices[2], target);

	Vec3d p1(vertices[0]->xPos(), vertices[0]->yPos(), z1);
	Vec3d p2(vertices[1]->xPos(), vertices[1]->yPos(), z2);
	Vec3d p3(vertices[2]->xPos(), vertices[2]->yPos(), z3);
	target->setPlane<T>(MathStuff::planeFromTriangle(p1, p2, p3));
}
