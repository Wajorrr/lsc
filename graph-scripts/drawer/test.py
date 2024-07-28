from gradient_analyze import gradient_analyze

bars_dir1 = {
    'Ridge': {
        'color': 'limegreen',
        'hatch': None,
        'y': [],
        'label': 'Ridge',
    },
    'Lasso': {
        'color': 'olive',
        'hatch': None,
        'y': [],
        'label': 'Lasso',
    },
    'Neural Network': {
        'color': 'darkorange',
        'hatch': None,
        'y': [],
        'label': 'Neural Network',
    },
    'CART': {
        'color': 'dodgerblue',
        'hatch': None,
        'y': [],
        'label': 'CART',
    },
    'XGBoost': {
        'color': 'blueviolet',
        'hatch': None,
        'y': [],
        'label': 'XGBoost',
    },
    'Random Forest': {
        'color': 'red',
        'hatch': None,
        'y': [],
        'label': 'Random Forest',
    }
}
bars_dir2 = {
    'Ridge': {
        'color': 'limegreen',
        'hatch': None,
        'y': [],
        'label': 'Ridge',
    },
    'Lasso': {
        'color': 'olive',
        'hatch': None,
        'y': [],
        'label': 'Lasso',
    },
    'Neural Network': {
        'color': 'darkorange',
        'hatch': None,
        'y': [],
        'label': 'Neural Network',
    },
    'CART': {
        'color': 'dodgerblue',
        'hatch': None,
        'y': [],
        'label': 'CART',
    },
    'XGBoost': {
        'color': 'blueviolet',
        'hatch': None,
        'y': [],
        'label': 'XGBoost',
    },
    'Random Forest': {
        'color': 'red',
        'hatch': None,
        'y': [],
        'label': 'Random Forest',
    }
}
indicator2label = {
    'iops': ('vdbench', 'iops'),
    'bandwidth': ('vdbench', 'bandwidth'),
    'latency': ('vdbench', 'avg_resp'),
}

if __name__ == '__main__':
    gradient_analyze()
