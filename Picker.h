#pragma once

/*
	Scene object picking with mouse raycasts in viewspace.
	Note: Gameobjects are currently stored in Scene class, currently no way to validate destroyed objects, 
		  will need to use weak pointers or finish experimental "DataArray" class
*/

#include <utility> // for std::pair
#include <vector> 

class Scene;
class GameObject;

class Picker {

	friend class EditorGui;

public:
	Picker();

	// Raycast intersection test with all objects in a scene, results are cached and cleared when mouse moves.
	// Only checks GameObject type and assumes it has a valid AABB
	GameObject* MouseRaycast(Scene& scene, int mouseX, int mouseY, int windowWidth, int windowHeight);

	// Note: returned object not const
	GameObject* GetPickedObject() const { return m_PickedObject; };

	void ClearPickedObject();

private:
	GameObject* m_PickedObject {};

	std::vector<std::pair<float, GameObject*>> m_RaycastCache {};
	std::pair<int, int>                        m_LastMousePos {};
};

