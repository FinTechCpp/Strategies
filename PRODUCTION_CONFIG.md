# Exemple de Configuration pour Production

## Pour une version optimisée sans logs

Si vous souhaitez compiler une version de production **sans aucun overhead de logging**, suivez ces étapes :

### 1. Modifier le fichier `strategy.h`

Dans `ThirdParty/Strategies/include/strategy.h`, décommentez la ligne suivante :

```cpp
// Avant (mode développement - avec logs)
// #define STRATEGY_DISABLE_LOGGING

// Après (mode production - sans logs)
#define STRATEGY_DISABLE_LOGGING
```

### 2. Recompiler en mode release

```bash
./build_and_run.sh --clean --no-run
```

### 3. Résultat attendu

Avec `STRATEGY_DISABLE_LOGGING` activé :
- **Aucun** appel aux méthodes de logging n'est compilé
- **Aucune** construction de chaînes de log
- **Aucun** overhead de performance
- Le code généré est optimal

### 4. Vérification

Vous pouvez vérifier l'impact en compilant avec et sans le define, puis en comparant :

```bash
# Taille du binaire
ls -lh build-release/backtestApp/backtestapp

# Performances avec un profiler
perf stat ./build-release/backtestApp/backtestapp
```

### Benchmark attendu

| Configuration | Overhead | Notes |
|--------------|----------|-------|
| Logs activés (utilisateur) | ~100% | Construction complète des logs |
| Logs désactivés (utilisateur) | ~5-10% | Appels de fonction vides |
| `STRATEGY_DISABLE_LOGGING` | **0%** | Aucun code de logging compilé |

## Configuration recommandée

### Développement
```cpp
// #define STRATEGY_DISABLE_LOGGING
```
- Garde tous les logs pour le debug
- Permet le profiling et l'analyse

### Production / Release
```cpp
#define STRATEGY_DISABLE_LOGGING
```
- Performance maximale
- Binaire optimisé
- Pas de risque de fuite d'informations via les logs

### Benchmark / Profiling
Alternez entre les deux modes pour mesurer l'impact exact des logs sur vos cas d'usage spécifiques.
