//-----------------------------------------------------------------------------
// Stable, human-readable identifiers for menu commands.
//
// These strings are persisted in user keymap files: never rename one, only add.
// Submenu headers (OPEN_RECENT, GROUP_RECENT, LOCALE) are not invocable and have no name.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "commandnames.h"

namespace SolveSpace {

struct CommandNameEntry {
    Command     cmd;
    const char *name;
};

static const CommandNameEntry CommandNames[] = {
    // File
    { Command::NEW,               "new"                },
    { Command::OPEN,              "open"               },
    { Command::SAVE,              "save"               },
    { Command::SAVE_AS,           "save-as"            },
    { Command::EXPORT_IMAGE,      "export-image"       },
    { Command::EXPORT_MESH,       "export-mesh"        },
    { Command::EXPORT_PARTS,      "export-parts"       },
    { Command::EXPORT_PARTS_LIST, "export-parts-list"  },
    { Command::EXPORT_SURFACES,   "export-surfaces"    },
    { Command::EXPORT_VIEW,       "export-view"        },
    { Command::EXPORT_SECTION,    "export-section"     },
    { Command::EXPORT_WIREFRAME,  "export-wireframe"   },
    { Command::IMPORT,            "import"             },
    { Command::EXIT,              "exit"               },
    // View
    { Command::ZOOM_IN,           "zoom-in"            },
    { Command::ZOOM_OUT,          "zoom-out"           },
    { Command::ZOOM_TO_FIT,       "zoom-to-fit"        },
    { Command::SHOW_GRID,         "show-grid"          },
    { Command::DIM_SOLID_MODEL,   "dim-solid-model"    },
    { Command::PERSPECTIVE_PROJ,  "perspective"        },
    { Command::EXPLODE_SKETCH,    "explode-sketch"     },
    { Command::ONTO_WORKPLANE,    "align-to-workplane" },
    { Command::NEAREST_ORTHO,     "nearest-ortho"      },
    { Command::NEAREST_ISO,       "nearest-iso"        },
    { Command::CENTER_VIEW,       "center-view"        },
    { Command::SHOW_TOOLBAR,      "show-toolbar"       },
    { Command::SHOW_TEXT_WND,     "show-property-browser" },
    { Command::UNITS_INCHES,      "units-inches"       },
    { Command::UNITS_FEET_INCHES, "units-feet-inches"  },
    { Command::UNITS_MM,          "units-mm"           },
    { Command::UNITS_METERS,      "units-meters"       },
    { Command::FULL_SCREEN,       "full-screen"        },
    // Edit
    { Command::UNDO,              "undo"               },
    { Command::REDO,              "redo"               },
    { Command::CUT,               "cut"                },
    { Command::COPY,              "copy"               },
    { Command::PASTE,             "paste"              },
    { Command::PASTE_TRANSFORM,   "paste-transformed"  },
    { Command::DELETE,            "delete"             },
    { Command::SELECT_CHAIN,      "select-edge-chain"  },
    { Command::SELECT_ALL,        "select-all"         },
    { Command::SNAP_TO_GRID,      "snap-to-grid"       },
    { Command::ROTATE_90,         "rotate-imported-90" },
    { Command::UNSELECT_ALL,      "unselect-all"       },
    { Command::REGEN_ALL,         "regenerate-all"     },
    { Command::EDIT_LINE_STYLES,  "line-styles"        },
    { Command::VIEW_PROJECTION,   "view-projection"    },
    { Command::CONFIGURATION,     "configuration"      },
    // Request
    { Command::SEL_WORKPLANE,     "in-workplane"       },
    { Command::FREE_IN_3D,        "free-in-3d"         },
    { Command::DATUM_POINT,       "datum-point"        },
    { Command::WORKPLANE,         "workplane"          },
    { Command::LINE_SEGMENT,      "line-segment"       },
    { Command::CONSTR_SEGMENT,    "construction-line-segment" },
    { Command::CIRCLE,            "circle"             },
    { Command::ARC,               "arc"                },
    { Command::RECTANGLE,         "rectangle"          },
    { Command::CUBIC,             "cubic-spline"       },
    { Command::TTF_TEXT,          "ttf-text"           },
    { Command::IMAGE,             "image"              },
    { Command::SPLIT_CURVES,      "split-curves"       },
    { Command::TANGENT_ARC,       "tangent-arc"        },
    { Command::CONSTRUCTION,      "toggle-construction" },
    // Group
    { Command::GROUP_3D,          "group-sketch-3d"    },
    { Command::GROUP_WRKPL,       "group-sketch-workplane" },
    { Command::GROUP_EXTRUDE,     "group-extrude"      },
    { Command::GROUP_HELIX,       "group-helix"        },
    { Command::GROUP_LATHE,       "group-lathe"        },
    { Command::GROUP_REVOLVE,     "group-revolve"      },
    { Command::GROUP_ROT,         "group-step-rotate"  },
    { Command::GROUP_TRANS,       "group-step-translate" },
    { Command::GROUP_LINK,        "group-link"         },
    { Command::GROUP_FILLET,      "group-fillet"       },
    { Command::GROUP_CHAMFER,     "group-chamfer"      },
    { Command::GROUP_SHELL,       "group-shell"        },
    { Command::GROUP_DRAFT,       "group-draft"        },
    { Command::GROUP_HOLE,        "group-hole"         },
    { Command::GROUP_LOFT,        "group-loft"         },
    { Command::GROUP_SWEEP,       "group-sweep"        },
    { Command::PART_NEW,          "part-new"           },
    { Command::JOINT_REVOLUTE,    "joint-revolute"     },
    { Command::JOINT_SLIDER,      "joint-slider"       },
    { Command::JOINT_CYLINDRICAL, "joint-cylindrical"  },
    { Command::JOINT_PLANAR,      "joint-planar"       },
    { Command::JOINT_BALL,        "joint-ball"         },
    { Command::JOINT_GEAR,        "joint-gear"         },
    { Command::GROUP_FIT,         "group-fit"          },
    { Command::GROUP_EXTRUDE_CUT, "group-extrude-cut"  },
    { Command::GROUP_SPLIT,       "group-split"        },
    { Command::DESIGN_POSE,       "design-pose"        },
    // Constrain
    { Command::DISTANCE_DIA,      "constrain-distance" },
    { Command::REF_DISTANCE,      "constrain-reference-distance" },
    { Command::ANGLE,             "constrain-angle"    },
    { Command::REF_ANGLE,         "constrain-reference-angle" },
    { Command::OTHER_ANGLE,       "other-supplementary-angle" },
    { Command::REFERENCE,         "toggle-reference"   },
    { Command::EQUAL,             "constrain-equal"    },
    { Command::RATIO,             "constrain-ratio"    },
    { Command::DIFFERENCE,        "constrain-difference" },
    { Command::ON_ENTITY,         "constrain-on-entity" },
    { Command::SYMMETRIC,         "constrain-symmetric" },
    { Command::AT_MIDPOINT,       "constrain-midpoint" },
    { Command::HORIZONTAL,        "constrain-horizontal" },
    { Command::VERTICAL,          "constrain-vertical" },
    { Command::PARALLEL,          "constrain-parallel" },
    { Command::PERPENDICULAR,     "constrain-perpendicular" },
    { Command::ORIENTED_SAME,     "constrain-same-orientation" },
    { Command::WHERE_DRAGGED,     "constrain-where-dragged" },
    { Command::COMMENT,           "comment"            },
    // Analyze
    { Command::VOLUME,            "measure-volume"     },
    { Command::AREA,              "measure-area"       },
    { Command::PERIMETER,         "measure-perimeter"  },
    { Command::INTERFERENCE,      "show-interference"  },
    { Command::NAKED_EDGES,       "show-naked-edges"   },
    { Command::SHOW_DOF,          "show-dof"           },
    { Command::MASS_PROPERTIES,   "mass-properties"    },
    { Command::PARTS_LIST,        "parts-list"         },
    { Command::CENTER_OF_MASS,    "show-center-of-mass" },
    { Command::TRACE_PT,          "trace-point"        },
    { Command::STOP_TRACING,      "stop-tracing"       },
    { Command::STEP_DIM,          "step-dimension"     },
    // Help
    { Command::WEBSITE,           "website"            },
    { Command::GITHUB,            "github-commit"      },
    { Command::ABOUT,             "about"              },
};

const char *CommandName(Command cmd) {
    for(const CommandNameEntry &entry : CommandNames) {
        if(entry.cmd == cmd) return entry.name;
    }
    return nullptr;
}

Command CommandFromName(const std::string &name) {
    for(const CommandNameEntry &entry : CommandNames) {
        if(name == entry.name) return entry.cmd;
    }
    return Command::NONE;
}

}
