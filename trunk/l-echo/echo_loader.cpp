// echo_loader.cpp

/*
    This file is part of L-Echo.

    L-Echo is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    L-Echo is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with L-Echo.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <typeinfo>

#include <vector>
#include <map>

#include <stair.h>
#include <t_grid.h>
#include <escgrid.h>
#include <hole.h>
#include <grid.h>
#include <echo_stage.h>
#include <tinyxml/tinyxml.h>

class functor
{
    public:
        grid* obj;
        void (grid::*funcp)(grid*);
        functor(){}
        functor(grid* my_obj, void (grid::*my_funcp)(grid*))
        {
            obj = my_obj;
            funcp = my_funcp;
        }
        virtual void call(grid* ptr) const
        {
            (obj->*funcp)(ptr);
        }
};

class t_functor : public functor
{
    public:
        t_grid* t_obj;
        void (t_grid::*t_funcp)(grid*);
        t_functor(t_grid* my_obj, void (t_grid::*my_funcp)(grid*))
        {
            t_obj = my_obj;
            t_funcp = my_funcp;
        }
        virtual void call(grid* ptr) const
        {
            (t_obj->*t_funcp)(ptr);
        }
};

typedef std::vector<functor> GRID_PTR_SET;
typedef std::map<std::string, GRID_PTR_SET*> DEPENDENCY_MAP;

static grid* parse_grid(TiXmlElement* txe, stage* st, DEPENDENCY_MAP* map, int add_to_map);

stage* load_stage(const char* file_name)
{
    TiXmlDocument doc(file_name);
    if(doc.LoadFile())
    {
	    DEPENDENCY_MAP* map = new DEPENDENCY_MAP();
        stage* ret = new stage();
        TiXmlElement* root = doc.RootElement();
        TiXmlNode* child = NULL;
        while(child = root->IterateChildren(child))
        {
            parse_grid(child->ToElement(), ret, map, 1);
        }
        ret->set_start(ret->get(root->Attribute("start")));
        ret->set_name(*(new std::string(root->Attribute("name"))));
        int num_goals = 0;
        root->QueryIntAttribute("goals", &num_goals);
        ret->set_num_goals(num_goals);
        std::cout << "is map empty?: " << map->empty() << std::endl;
        return(ret);
    }
    return(NULL);
}

static GRID_PTR_SET* dep_set(DEPENDENCY_MAP* map, char* id)
{
    std::string s1(id);
	DEPENDENCY_MAP::iterator it = map->find(s1);
	if(it == map->end())
	{
		return(NULL);
	}
	return(it->second);
}

static void add(DEPENDENCY_MAP* map, char* id, functor f)
{
	GRID_PTR_SET* set = dep_set(map, id);
	if(set)
	{
	    std::cout << "dep set for " << id << " found, adding"<< std::endl;
        set->push_back(f);
	}
	else
	{
	    std::cout << "dep set for " << id << " NOT found, adding"<< std::endl;
		set = new GRID_PTR_SET();
		set->push_back(f);
		map->insert(DEPENDENCY_MAP::value_type(id, set));
	}
}

static void add(DEPENDENCY_MAP* map, char* id, t_grid* obj, void (t_grid::*funcp)(grid*))
{
    t_functor f(obj, funcp);
	add(map, id, f);
}

static void add(DEPENDENCY_MAP* map, char* id, grid* obj, void (grid::*funcp)(grid*))
{
    functor f(obj, funcp);
	add(map, id, f);
}

static grid* parse_grid(TiXmlElement* txe, stage* st, DEPENDENCY_MAP* map, int add_to_map)
{
    std::cout << std::endl;
    const char* type = txe->Value();
    //*
    grid_info_t* info = new(grid_info_t);
    txe->QueryFloatAttribute("x", &info->pos.x);
    txe->QueryFloatAttribute("y", &info->pos.y);
    txe->QueryFloatAttribute("z", &info->pos.z);

    char* name = const_cast<char*>(txe->Attribute("id"));
    char* prev_id = const_cast<char*>(txe->Attribute("prev"));
    grid* prev = st->get(prev_id);
    char* next_id = const_cast<char*>(txe->Attribute("next"));
    grid* next = st->get(next_id);

    grid* new_grid = NULL;
    // */
    if(!strcmp(type, "grid"))
    {
	    std::cout << name << " is a grid!" << std::endl;
	    new_grid = new grid(info, prev, next);
    }
    else if(!strcmp(type, "t_grid"))
    {
        std::cout << name << " is a t_grid!" << std::endl;
        char* next2_id = const_cast<char*>(txe->Attribute("next2"));
        grid* next2 = st->get(next2_id);
        new_grid = new t_grid(info, prev, next, next2);
	    if(!next2)
	    {
	        std::cout << "next2 (" << next2_id << ") is null, adding to dep map" << std::endl;
		    add(map, next2_id, (t_grid*)new_grid, &t_grid::set_real_next2);
	    }
    }
    else if(!strcmp(type, "escgrid"))
    {
        std::cout << name << " is an escgrid!" << std::endl;
        new_grid = new escgrid(info, prev, next);
	    if(txe->FirstChild())
	    {
            TiXmlElement* child = txe->FirstChild()->ToElement();
            while(child)
            {
                vector3f* each_angle = new vector3f();
                child->QueryFloatAttribute("x", &each_angle->x);
                child->QueryFloatAttribute("y", &each_angle->y);
                child->QueryFloatAttribute("z", &each_angle->z);
                grid* each_esc = parse_grid(child->FirstChild()->ToElement(), st, map, 0);
                ((escgrid*)new_grid)->add(each_angle, each_esc);
                child = child->NextSiblingElement();
            }
	    }
    }
    else if(!strcmp(type, "hole"))
    {
        std::cout << name << " is an hole!" << std::endl;
        new_grid = new hole(info, prev, next);
        if(txe->FirstChild())
	    {
            TiXmlElement* child = txe->FirstChild()->ToElement();
            while(child)
            {
                vector3f* each_angle = new vector3f();
                child->QueryFloatAttribute("x", &each_angle->x);
                child->QueryFloatAttribute("y", &each_angle->y);
                child->QueryFloatAttribute("z", &each_angle->z);
                grid* each_esc = parse_grid(child->FirstChild()->ToElement(), st, map, 0);
                ((hole*)new_grid)->add(each_angle, each_esc);
                child = child->NextSiblingElement();
            }
	    }
    }
    else if(!strcmp(type, "stair"))
    {
        std::cout << name << " is a stair!" << std::endl;
        TiXmlElement* dir_element = txe->FirstChild()->ToElement();
        vector3f* dir_angle = new vector3f();
        dir_element->QueryFloatAttribute("x", &dir_angle->x);
        dir_element->QueryFloatAttribute("y", &dir_angle->y);
        dir_element->QueryFloatAttribute("z", &dir_angle->z);
        TiXmlElement* width_element = dir_element->NextSiblingElement();
        vector3f* width_angle = new vector3f();
        width_element->QueryFloatAttribute("x", &width_angle->x);
        width_element->QueryFloatAttribute("y", &width_angle->y);
        width_element->QueryFloatAttribute("z", &width_angle->z);
        new_grid = new stair(info, prev, next, dir_angle, width_angle);
    }
    if(!prev)
    {
        std::cout << "prev (" << prev_id << ") is null, adding to dep map" << std::endl;
        add(map, prev_id, new_grid, &grid::set_real_prev);
    }
    if(!next)
    {
        std::cout << "next (" << next_id << ") is null, adding to dep map" << std::endl;
        add(map, next_id, new_grid, &grid::set_real_next);
    }
    if(txe->Attribute("goal"))
    {
        int is_goal = 0;
        txe->QueryIntAttribute("goal", &is_goal);
        if(is_goal)
        {
            new_grid->set_as_goal();
            std::cout << "it's a goal!" <<std::endl;
        }
    }
    GRID_PTR_SET* deps = dep_set(map, name);
    if(deps)
    {
	    std::cout << "deps found for: " << name << std::endl;
	    GRID_PTR_SET::iterator it = deps->begin(), end = deps->end();
	    while(it != end)
	    {
            it->call(new_grid);
            std::cout << "dep called for " << it->obj << " (" << typeid(*(it->obj)).name() << ") by " << name << std::endl;
	        it++;
	    }
	    map->erase(name);
    }
    else
        std::cout << "deps not found for: " << name << std::endl;
    if(add_to_map)
        st->add(name, new_grid);
    return(new_grid);
}