// Example usage of hegel.hpp
#include <hegel/hegel.hpp>
#include <string>
#include <vector>

// Define some types that reflect-cpp can work with
struct Person {
  std::string name;
  int age;
};

struct Team {
  std::string team_name;
  std::vector<Person> members;
};

int main() {
  auto team_gen = hegel::default_generator<Team>();

  team_gen.schema() = R"json({
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "type": "object",
    "properties": {
        "team_name": {
            "type": "string",
            "minLength": 1,
            "maxLength": 50,
            "description": "The name of the team"
        },
        "members": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "name": {
                        "type": "string",
                        "minLength": 1,
                        "maxLength": 100,
                        "description": "Person's full name"
                    },
                    "age": {
                        "type": "integer",
                        "minimum": 10,
                        "maximum": 80,
                        "description": "Person's age (working age adult)"
                    }
                },
                "required": ["name", "age"],
                "additionalProperties": false
            },
            "minItems": 2,
            "maxItems": 10,
            "description": "Team members (2-10 people)"
        }
    },
    "required": ["team_name", "members"],
    "additionalProperties": false
})json";

  // Set a more constrained schema
  Team t = team_gen.generate();

  auto team = team_gen.generate();

  for (auto member : team.members) {
    if (member.age < 18) {
      std::cerr << "Member \"" << member.name
                << "\" is too young with an age of " << member.age << std::endl;
      exit(1);
    }
  }

  return 0;
}
