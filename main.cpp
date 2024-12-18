#include <raylib.h>
#include <stdlib.h>
#include <time.h> 
#include <limits> 
#include <fstream>
#include <string>
#include <cmath> 

using namespace std; 

// Constants
#define SCREEN_WIDTH 800    // Définition de la largeur de la fenêtre d'affichage (en pixels)
#define SCREEN_HEIGHT 600   // Définition de la hauteur de la fenêtre d'affichage (en pixels)
#define CELL_SIZE 40        // Définition de la taille d'une cellule du labyrinthe (en pixels)
#define GRID_WIDTH (SCREEN_WIDTH / CELL_SIZE)  // Calcul du nombre de cellules en largeur en fonction de la taille de la fenêtre et de la taille de la cellule
#define GRID_HEIGHT (SCREEN_HEIGHT / CELL_SIZE) // Calcul du nombre de cellules en hauteur en fonction de la taille de la fenêtre et de la taille de la cellule
// Structures
class Position {  // Définition d'une classe représentant une position avec des coordonnées x et y
public:
    int x, y;  // Déclaration des variables membres x et y représentant la position

    // Constructeur qui initialise les valeurs x et y à 0 par défaut, mais peut être modifié avec des valeurs spécifiques
    Position(int x = 0, int y = 0) : x(x), y(y) {}
}; 

class Cell {
public:
    bool visited;           // Variable indiquant si la cellule a été visitée ou non (utile pour l'algorithme de génération de labyrinthes)
    bool topWall, bottomWall, leftWall, rightWall; // Variables pour les murs de la cellule (chaque mur est représenté par un booléen)

    // Constructeur par défaut
    Cell() : visited(false), topWall(true), bottomWall(true), leftWall(true), rightWall(true) {}
    // Le constructeur initialise les valeurs des membres de la classe :
    // - visited est initialisé à false (la cellule n'a pas été visitée)
    // - tous les murs (haut, bas, gauche, droite) sont initialisés à true (les murs sont présents)
};

class Niveau {
public:
    enum Level { FACILE, MOYEN, DIFFICILE };  // Définition d'un énuméré pour les trois niveaux de difficulté
    Level niveau;  // Variable membre qui représente le niveau actuel

    // Constructeur qui initialise le niveau à MOYEN par défaut, mais permet de définir un autre niveau si nécessaire
    Niveau(Level level = MOYEN) : niveau(level) {}

    // Fonction qui retourne la densité des obstacles en fonction du niveau choisi
    float getObstacleDensity() {
        switch (niveau) {
            case FACILE: return 0.3f;   // Pour le niveau facile, la densité des obstacles est de 30%
            case MOYEN: return 0.5f;    // Pour le niveau moyen, la densité des obstacles est de 50%
            case DIFFICILE: return 0.7f; // Pour le niveau difficile, la densité des obstacles est de 70%
        }
    }

    // Fonction qui indique si le niveau est dynamique (seulement le niveau difficile est dynamique)
    bool isDynamic() {
        return niveau == DIFFICILE; // Si le niveau est difficile, la fonction retourne true, sinon false
    }
};

class Game; // Forward declaration

class Obstacle {
public:
    Position position;        // La position de l'obstacle dans le labyrinthe (utilise la classe Position pour gérer les coordonnées x et y)
    float moveTimer;          // Un compteur de temps qui permet de contrôler le déplacement de l'obstacle
    float moveInterval;       // L'intervalle de temps entre chaque déplacement de l'obstacle (en secondes)
    Texture2D texture;        // Texture de l'obstacle (ex. image qui représente l'obstacle)

    // Constructeur qui initialise la position, le timer de mouvement, l'intervalle et la texture de l'obstacle
    Obstacle(int x = 0, int y = 0, float interval = 100.0f, const char* texturePath = "Spike.png") 
        : position(x, y), moveTimer(0), moveInterval(interval) {
        texture = LoadTexture(texturePath); // Charge la texture spécifiée pour l'obstacle
    }

    // Destructeur qui décharge la texture quand l'obstacle n'est plus nécessaire
    ~Obstacle() {
        UnloadTexture(texture);  // Décharge la texture de la mémoire
    }

    // Fonction qui permet de changer la texture de l'obstacle
    void SetTexture(const char* texturePath) {
        UnloadTexture(texture);  // Décharge la texture actuelle
        texture = LoadTexture(texturePath);  // Charge la nouvelle texture
    }

    // Fonction qui fait déplacer l'obstacle dans le labyrinthe
    void Move() {

        moveTimer += GetFrameTime();  // Incrémente le timer de mouvement en fonction du temps écoulé entre deux images
    
        if (moveTimer >= moveInterval) {  // Si l'intervalle de déplacement est atteint
            // Déplace l'obstacle dans une direction aléatoire
            position.x += GetRandomValue(-2, 2);
            position.y += GetRandomValue(-2, 2);

            // Limite les déplacements de l'obstacle pour qu'il reste dans les limites du labyrinthe
            if (position.x < 0) position.x = 0;
            if (position.x >= GRID_WIDTH) position.x = GRID_WIDTH - 1;
            if (position.y < 0) position.y = 0;
            if (position.y >= GRID_HEIGHT) position.y = GRID_HEIGHT - 1;

            moveTimer = 0;  // Réinitialise le timer pour le prochain déplacement
        }
    }

    // Fonction qui dessine l'obstacle à l'écran avec mise à l'échelle et décalage
    void Draw(float scaleFactor, int offsetX, int offsetY) {
        int scaledCellSize = scaleFactor * SCREEN_WIDTH / GRID_WIDTH;  // Calcul de la taille de la cellule mise à l'échelle
        int posX = offsetX + position.x * scaledCellSize;  // Calcul de la position horizontale de l'obstacle
        int posY = offsetY + position.y * scaledCellSize;  // Calcul de la position verticale de l'obstacle

        // Dessine la texture de l'obstacle à la position spécifiée
        DrawTexturePro(texture, {0, 0, (float)texture.width, (float)texture.height},
            {(float)posX, (float)posY, (float)scaledCellSize, (float)scaledCellSize}, {0, 0}, 0, WHITE);
    }

    // Fonction qui vérifie si l'obstacle est en collision avec le joueur
    bool CheckCollision(Position player) {
        // Retourne true si l'obstacle se trouve à la même position que le joueur
        return (position.x == player.x && position.y == player.y);
    }
};

class Maze {
private:
    Cell maze[GRID_WIDTH][GRID_HEIGHT];    // Tableau de cellules représentant le labyrinthe
    Niveau niveau;                         // Niveau du jeu, définissant la difficulté
    Obstacle movingObstacle;               // Obstacle qui se déplace dans le labyrinthe
    int gridWidth, gridHeight;             // Dimensions du labyrinthe
    Texture2D wallTexture;                 // Texture des murs du labyrinthe

    // Génère un chemin dans le labyrinthe en utilisant un algorithme de backtracking
    void GeneratePath(int x, int y) {
        maze[x][y].visited = true;  // Marque la cellule actuelle comme visitée
        while (true) {
            // Tableau de directions possibles : 0 = haut, 1 = droite, 2 = bas, 3 = gauche
            int directions[] = {0, 1, 2, 3}; 
            
            // Mélange aléatoirement les directions pour diversifier le parcours
            for (int i = 0; i < 4; i++) {
                int j = GetRandomValue(i, 3);
                int temp = directions[i];
                directions[i] = directions[j];
                directions[j] = temp;
            }

            bool moved = false;  // Indicateur si un mouvement a été effectué
            for (int i = 0; i < 4; i++) {
                int nx = x, ny = y;
                // Calcul de la nouvelle position en fonction de la direction
                if (directions[i] == 0) ny -= 1;  // Haut
                else if (directions[i] == 1) nx += 1;  // Droite
                else if (directions[i] == 2) ny += 1;  // Bas
                else if (directions[i] == 3) nx -= 1;  // Gauche

                // Vérifie si la nouvelle position est valide et si la cellule n'est pas visitée
                if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight && !maze[nx][ny].visited) {
                    // Supprime les murs entre la cellule actuelle et la cellule voisine
                    if (directions[i] == 0) {
                        maze[x][y].topWall = false;
                        maze[nx][ny].bottomWall = false;
                    } else if (directions[i] == 1) {
                        maze[x][y].rightWall = false;
                        maze[nx][ny].leftWall = false;
                    } else if (directions[i] == 2) {
                        maze[x][y].bottomWall = false;
                        maze[nx][ny].topWall = false;
                    } else if (directions[i] == 3) {
                        maze[x][y].leftWall = false;
                        maze[nx][ny].rightWall = false;
                    }
                    GeneratePath(nx, ny);  // Récursion pour continuer à générer le chemin
                    moved = true;
                    break;  // Sort de la boucle dès qu'un mouvement est effectué
                }
            }
            if (!moved) break;  // Si aucun mouvement n'est possible, on arrête le processus
        }
    }

public:
    // Retourne une référence à l'obstacle qui se déplace dans le labyrinthe
    Obstacle& GetMovingObstacle() {
     return movingObstacle;  // Retourne l'obstacle mobile
    }

    // Constructeur qui initialise le labyrinthe avec la difficulté choisie et génère le chemin
    Maze(Niveau::Level level) : niveau(level), movingObstacle(0, 0, 0.5f) {
        gridWidth = GRID_WIDTH;
        gridHeight = GRID_HEIGHT;

        wallTexture = LoadTexture("brick.png");  // Charger la texture des murs
        InitializeMaze();  // Initialise le labyrinthe avec des murs
        GeneratePath(0, 0);  // Génère le chemin à partir de la position initiale (0, 0)
    }

    // Destructeur qui libère la mémoire utilisée par la texture des murs
    ~Maze() {
        UnloadTexture(wallTexture);  // Libère la texture utilisée pour les murs
    }

    // Initialise toutes les cellules du labyrinthe avec des murs et non visitées
    void InitializeMaze() {
        for (int x = 0; x < gridWidth; x++) {
            for (int y = 0; y < gridHeight; y++) {
                maze[x][y].visited = false;  // Marque la cellule comme non visitée
                maze[x][y].topWall = true;   // Initialise le mur supérieur
                maze[x][y].bottomWall = true; // Initialise le mur inférieur
                maze[x][y].leftWall = true;  // Initialise le mur gauche
                maze[x][y].rightWall = true; // Initialise le mur droit
            }
        }
    }

    // Regénère le labyrinthe à partir de la position actuelle du joueur
    void Regenerate(Position playerPosition) {
        InitializeMaze();  // Réinitialise le labyrinthe
        GeneratePath(playerPosition.x, playerPosition.y);  // Re-génère un nouveau chemin à partir de la position du joueur
    }

    // Dessine le labyrinthe à l'écran
    void DrawMaze(Position player) {
        float scaleFactor = 0.75f; // Facteur d'échelle pour ajuster la taille du labyrinthe
        int reducedWidth = scaleFactor * SCREEN_WIDTH;  // Largeur réduite en fonction du facteur d'échelle
        int reducedHeight = scaleFactor * SCREEN_HEIGHT; // Hauteur réduite en fonction du facteur d'échelle

        int offsetX = (SCREEN_WIDTH - reducedWidth) / 2; // Décalage horizontal pour centrer le labyrinthe
        int offsetY = (SCREEN_HEIGHT - reducedHeight) / 2; // Décalage vertical pour centrer le labyrinthe

        int scaledCellSize = reducedWidth / GRID_WIDTH;  // Taille des cellules mise à l'échelle
        int lineThickness = 4;  // Épaisseur des murs du labyrinthe

        // Dessine chaque cellule du labyrinthe
        for (int x = 0; x < gridWidth; x++) {
            for (int y = 0; y < gridHeight; y++) {
                int posX = offsetX + x * scaledCellSize;
                int posY = offsetY + y * scaledCellSize;

                // Dessine le mur supérieur
                if (maze[x][y].topWall) {
                    DrawTexturePro(
                        wallTexture,
                        (Rectangle){0, 0, wallTexture.width, wallTexture.height},
                        (Rectangle){posX, posY, scaledCellSize, lineThickness},
                        (Vector2){0, 0},
                        0.0f,
                        WHITE
                    );
                }
                // Dessine le mur droit
                if (maze[x][y].rightWall) {
                    DrawTexturePro(
                        wallTexture,
                        (Rectangle){0, 0, wallTexture.width, wallTexture.height},
                        (Rectangle){posX + scaledCellSize - lineThickness, posY, lineThickness, scaledCellSize},
                        (Vector2){0, 0},
                        0.0f,
                        WHITE
                    );
                }
                // Dessine le mur inférieur
                if (maze[x][y].bottomWall) {
                    DrawTexturePro(
                        wallTexture,
                        (Rectangle){0, 0, wallTexture.width, wallTexture.height},
                        (Rectangle){posX, posY + scaledCellSize - lineThickness, scaledCellSize, lineThickness},
                        (Vector2){0, 0},
                        0.0f,
                        WHITE
                    );
                }
                // Dessine le mur gauche
                if (maze[x][y].leftWall) {
                    DrawTexturePro(
                        wallTexture,
                        (Rectangle){0, 0, wallTexture.width, wallTexture.height},
                        (Rectangle){posX, posY, lineThickness, scaledCellSize},
                        (Vector2){0, 0},
                        0.0f,
                        WHITE
                    );
                }
            }
        }

        // Si le niveau est moyen, déplacer et dessiner l'obstacle mobile
        if (niveau.niveau == Niveau::MOYEN) {
            movingObstacle.Move();  // Déplace l'obstacle
            movingObstacle.Draw(scaleFactor, offsetX, offsetY);  // Dessine l'obstacle
        }
    }

    // Vérifie s'il y a un mur dans la direction donnée par dx et dy
    bool HasWall(Position player, int dx, int dy) {
        if (dx == -1 && maze[player.x][player.y].leftWall) return true;  // Vérifie le mur gauche
        if (dx == 1 && maze[player.x][player.y].rightWall) return true;  // Vérifie le mur droit
        if (dy == -1 && maze[player.x][player.y].topWall) return true;   // Vérifie le mur supérieur
        if (dy == 1 && maze[player.x][player.y].bottomWall) return true; // Vérifie le mur inférieur
        return false;  // Si aucun mur, retourne false
    }

    // Accesseurs pour obtenir la largeur et la hauteur du labyrinthe
    int GetGridWidth() const { return gridWidth; }
    int GetGridHeight() const { return gridHeight; }
};

class Player {
public:
    Position position;            // Position actuelle du joueur dans le labyrinthe
    int gridWidth, gridHeight;    // Dimensions dynamiques du labyrinthe
    Texture2D texture;            // Texture pour représenter le joueur

    // Constructeur initialisant la position et la texture du joueur
    Player(int x = 0, int y = 0, const char* texturePath = "Tom.png") 
        : position(x, y), gridWidth(GRID_WIDTH), gridHeight(GRID_HEIGHT) {
        // Charger la texture depuis un fichier
        texture = LoadTexture(texturePath);
    }

    // Destructeur pour décharger la texture lorsque l'objet est détruit
    ~Player() {
        UnloadTexture(texture);
    }

    // Définir la taille du labyrinthe pour le joueur
    void SetGridSize(int width, int height) {
        gridWidth = width;
        gridHeight = height;
    }

    // Fonction pour déplacer le joueur
    void Move(int dx, int dy, Maze &maze, bool &gameWon) {
        int newX = position.x + dx;
        int newY = position.y + dy;

        // Vérifie que le nouveau déplacement est à l'intérieur du labyrinthe
        if (newX < 0 || newX >= gridWidth || newY < 0 || newY >= gridHeight) return;

        // Vérifie si un mur bloque le déplacement
        if (maze.HasWall(position, dx, dy)) return;

        // Met à jour la position du joueur
        position.x = newX;
        position.y = newY;

        // Vérifie si le joueur a atteint la sortie
        if (position.x == gridWidth - 1 && position.y == gridHeight - 1) gameWon = true;
    }

    // Fonction pour dessiner le joueur à sa position actuelle dans le labyrinthe
    void Draw(float scaleFactor, int offsetX, int offsetY) {
        int scaledCellSize = scaleFactor * SCREEN_WIDTH / gridWidth;
        // Calcul de la position du joueur dans le labyrinthe centré
        int posX = offsetX + position.x * scaledCellSize;
        int posY = offsetY + position.y * scaledCellSize;
        int padding = 2; // Padding pour ajuster la taille du joueur

        // Dessine la texture du joueur à la position calculée
        DrawTexturePro(
            texture,
            {0, 0, (float)texture.width, (float)texture.height},  // Source de la texture
            {(float)posX, (float)posY, (float)scaledCellSize, (float)scaledCellSize}, // Destination
            {0, 0},  // Origine (aucun décalage)
            0,       // Pas de rotation
            WHITE    // Couleur blanche pour conserver l'image originale
        );
    }
};


void ShowIntroScreen() { 
    // Charger l'image de fond
    Texture2D background = LoadTexture("img2.png"); // Remplacez par le chemin de votre image

    // Initialiser le système audio
    InitAudioDevice(); 

    // Charger et jouer la musique en boucle
    Music introMusic = LoadMusicStream("tom-and-jerry-ringtone (online-audio-converter.com).wav"); // Remplacez par le chemin de votre fichier musique
    PlayMusicStream(introMusic); // Jouer la musique
    SetMusicVolume(introMusic, 0.5f); // Optionnel : ajuster le volume de la musique

    // Calculer l'échelle de l'image pour s'adapter à l'écran
    float scaleX = (float)SCREEN_WIDTH / (float)background.width;
    float scaleY = (float)SCREEN_HEIGHT / (float)background.height;
    float scale = (scaleX > scaleY) ? scaleX : scaleY; // Choisir l'échelle la plus adaptée pour ne pas déformer l'image

    // Définir le bouton "PLAY NOW"
    Rectangle playButtonBase = {SCREEN_WIDTH - 220, 20, 200, 60}; // Taille de base du bouton
    float buttonScale = 1.0f;   // Échelle dynamique du bouton
    float scaleSpeed = 0.5f;    // Vitesse d'oscillation
    bool isGrowing = true;      // Indique si le bouton est en train de grandir

    // Boucle principale de l'écran d'introduction
    while (!WindowShouldClose()) {
        // Mettre à jour la musique en boucle 
        UpdateMusicStream(introMusic); // Mise à jour de la musique en continu

        // Gérer l'oscillation de la taille du bouton
        if (isGrowing) {
            buttonScale += scaleSpeed * GetFrameTime(); // Augmenter la taille du bouton
            if (buttonScale >= 1.2f) { // Limite supérieure de l'échelle
                buttonScale = 1.2f;
                isGrowing = false; // Le bouton commence à diminuer
            }
        } else {
            buttonScale -= scaleSpeed * GetFrameTime(); // Diminuer la taille du bouton
            if (buttonScale <= 1.0f) { // Limite inférieure de l'échelle
                buttonScale = 1.0f;
                isGrowing = true; // Le bouton commence à augmenter
            }
        }

        // Calculer les dimensions actuelles du bouton en fonction de son échelle
        Rectangle playButton = {
            playButtonBase.x - ((buttonScale - 1.0f) * playButtonBase.width) / 2,
            playButtonBase.y - ((buttonScale - 1.0f) * playButtonBase.height) / 2,
            playButtonBase.width * buttonScale,
            playButtonBase.height * buttonScale
        };

        BeginDrawing();
        ClearBackground(BLACK); // Effacer l'écran avec une couleur noire

        // Dessiner l'image de fond
        DrawTextureEx(background, {0, 0}, 0.0f, scale, WHITE);

        // Dessiner le bouton "PLAY NOW"
        Color buttonColor = CheckCollisionPointRec(GetMousePosition(), playButton) ? YELLOW : GOLD; // Couleur change si survolé
        DrawRectangleRounded(playButton, 0.5f, 10, buttonColor); // Fond du bouton avec arrondis
        DrawText("PLAY NOW", 
                 playButton.x + playButton.width / 8, 
                 playButton.y + playButton.height / 4, 
                 30 * buttonScale, 
                 DARKBROWN); // Texte du bouton

        EndDrawing();

        // Gérer le clic sur le bouton
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), playButton)) {
            StopMusicStream(introMusic); // Arrêter la musique quand l'utilisateur clique sur le bouton
            break; // Passer à l'écran suivant
        }
    }

    // Décharger les ressources
    UnloadMusicStream(introMusic); // Décharger la musique
    UnloadTexture(background); // Décharger l'image de fond
    CloseAudioDevice(); // Fermer le système audio
}

Niveau::Level ShowLevelMenu() {
    // Charger l'image de fond
    Texture2D background = LoadTexture("img4.png");

    // Calculer l'échelle de l'image pour s'adapter à l'écran
    float scaleX = (float)SCREEN_WIDTH / (float)background.width;
    float scaleY = (float)SCREEN_HEIGHT / (float)background.height;

    // Choisir la plus petite échelle pour conserver les proportions
    float scale = scaleX;

    // Dimensions des boutons
    int buttonWidth = 300;
    int buttonHeight = 50;
    int buttonX = SCREEN_WIDTH / 2 - buttonWidth / 2;

    // Positions des boutons
    Rectangle easyButton = {buttonX, SCREEN_HEIGHT / 2 - 90, buttonWidth, buttonHeight};
    Rectangle mediumButton = {buttonX, SCREEN_HEIGHT / 2 - 30, buttonWidth, buttonHeight};
    Rectangle hardButton = {buttonX, SCREEN_HEIGHT / 2 + 30, buttonWidth, buttonHeight};

    // Couleurs des boutons
    Color easyColor = WHITE;
    Color mediumColor = WHITE;
    Color hardColor = WHITE;

    while (!WindowShouldClose()) {

        // Obtenir la position actuelle de la souris
        Vector2 mousePoint = GetMousePosition();

        // Changer la couleur si la souris survole un bouton
        Color easyHover = CheckCollisionPointRec(mousePoint, easyButton) ? LIGHTGRAY : easyColor;
        Color mediumHover = CheckCollisionPointRec(mousePoint, mediumButton) ? LIGHTGRAY : mediumColor;
        Color hardHover = CheckCollisionPointRec(mousePoint, hardButton) ? LIGHTGRAY : hardColor;

        BeginDrawing();
        ClearBackground(BLACK);  // Effacer l'écran avec une couleur noire

        // Dessiner l'image de fond
        DrawTexture(background, 0, 0, WHITE);
        DrawTextureEx(background, {0, 0}, 0.0f, scale, WHITE);

        // Afficher le titre du menu
        DrawText(" Select Difficulty Level ", SCREEN_WIDTH / 2 - 180, SCREEN_HEIGHT / 2 - 150, 30, BLACK);

        // Dessiner les boutons avec les couleurs survolées
        DrawRectangleRec(easyButton, easyHover);
        DrawText("Easy", easyButton.x + 120, easyButton.y + 15, 23, BLACK);

        DrawRectangleRec(mediumButton, mediumHover);
        DrawText("Medium", mediumButton.x + 110, mediumButton.y + 15, 23, BLACK);

        DrawRectangleRec(hardButton, hardHover);
        DrawText("Hard", hardButton.x + 120, hardButton.y + 15, 23, BLACK);

        EndDrawing();

        // Gestion des clics
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Vérifier quel bouton a été cliqué et retourner le niveau correspondant
            if (CheckCollisionPointRec(mousePoint, easyButton)) return Niveau::FACILE;
            if (CheckCollisionPointRec(mousePoint, mediumButton)) return Niveau::MOYEN;
            if (CheckCollisionPointRec(mousePoint, hardButton)) return Niveau::DIFFICILE;
        }
    }

    // Décharger la texture de l'image de fond
    UnloadTexture(background);
}

class Game {
private:
    Maze maze;  // Le labyrinthe du jeu
    Player player;  // Le joueur, représentant Tom
    Position goal;  // La position de l'objectif (fromage Jerry)
    bool gameWon;  // Indicateur si le jeu est gagné
    bool isPaused;  // Indicateur si le jeu est en pause
    float timer;  // Chronomètre du jeu
    float changeTimer;  // Timer pour régénérer le labyrinthe
    float bestTime;  // Meilleur temps du joueur
    Niveau niveau;  // Niveau de difficulté
    Rectangle resetButton;  // Bouton pour réinitialiser le jeu
    Texture2D resetButtonTexture;  // Texture du bouton Reset
    Rectangle homeButton;  // Bouton pour revenir à l'écran d'accueil
    Texture2D homeButtonTexture;  // Texture du bouton d'accueil
    Rectangle retryButton;  // Bouton pour recommencer
    Rectangle quitButton;  // Bouton pour quitter
    Rectangle pauseButton;  // Bouton pour mettre en pause
    Texture2D goalTexture;  // Texture pour le point d'arrivée (fromage)
    Texture2D timerIcon;  // Texture pour l'icône du timer
    Texture2D backgroundTexture;  // Texture pour l'arrière-plan du jeu
    Texture2D pauseTexture;  // Texture pour le bouton de pause
    Texture2D resumeTexture;  // Texture pour le bouton de reprise

public:
    // Constructeur de la classe Game
    Game(Niveau::Level level, const char* playerTexturePath = "Tom.png", const char* obstacleTexturePath = "Spike.png", 
         const char* goalTexturePath = "jerry.png", const char* timerIconPath = "magana.png", 
         const char* BackgroundTexturePath = "img4.png", const char* resetButtonTexturePath = "reset.png", 
         const char* homeButtonTexturePath = "home.png")
    : player(0, 0, playerTexturePath), gameWon(false), isPaused(false), timer(0), changeTimer(0), bestTime(-1), niveau(level), maze(level) {
        
        // Initialisation de l'objectif, boutons et autres textures
        goal = Position(GRID_WIDTH - 1, GRID_HEIGHT - 1);  // Position de l'objectif (fromage Jerry)
        resetButton = {SCREEN_WIDTH -730, 16, 42, 42};  // Position du bouton Reset
        homeButton = {SCREEN_WIDTH - 785,10,57,57};  // Position du bouton Home
        retryButton = {SCREEN_WIDTH / 2 - 70, SCREEN_HEIGHT / 2 + 80, 140, 40};  // Position du bouton Retry
        quitButton = {SCREEN_WIDTH / 2 - 70, SCREEN_HEIGHT / 2 + 140, 140, 40};  // Position du bouton Quit
        pauseButton = {SCREEN_WIDTH - 70, 20, 50, 50};  // Position du bouton Pause
        
        // Chargement des textures pour les éléments du jeu
        goalTexture = LoadTexture(goalTexturePath);  // Charger la texture de l'objectif
        backgroundTexture = LoadTexture(BackgroundTexturePath);  // Charger la texture de l'arrière-plan
        timerIcon = LoadTexture(timerIconPath);  // Charger l'icône du timer
        resetButtonTexture = LoadTexture(resetButtonTexturePath);  // Charger la texture du bouton Reset
        homeButtonTexture = LoadTexture(homeButtonTexturePath);  // Charger la texture du bouton Home
        pauseTexture = LoadTexture("pause60.png");  // Charger la texture du bouton Pause
        resumeTexture = LoadTexture("resume60.png");  // Charger la texture du bouton Resume

        // Charger la texture de l'obstacle du labyrinthe
        maze.GetMovingObstacle().SetTexture(obstacleTexturePath);

        // Charger le meilleur temps du fichier
        std::ifstream infile("best_time.txt");
        if (infile.is_open()) {
            infile >> bestTime;  // Lire le meilleur temps enregistré
            infile.close();
        }
    }

    // Destructeur pour libérer les ressources
    ~Game() {
        // Libérer les textures des éléments chargés dans le constructeur
        UnloadTexture(resetButtonTexture);  // Libérer la texture du bouton Reset
        UnloadTexture(homeButtonTexture);  // Libérer la texture du bouton Home
        UnloadTexture(goalTexture);  // Libérer la texture de l'objectif
        UnloadTexture(backgroundTexture);  // Libérer la texture de l'arrière-plan
        UnloadTexture(timerIcon);  // Libérer l'icône du timer
        UnloadTexture(pauseTexture);  // Libérer la texture du bouton Pause
        UnloadTexture(resumeTexture);  // Libérer la texture du bouton Resume
    }

    // Fonction pour afficher l'écran d'introduction et réinitialiser le jeu
    void GoToIntroScreen() {
        // Réinitialiser tous les paramètres nécessaires avant de commencer
        ResetGame();  // Appeler la fonction ResetGame pour tout réinitialiser
        bestTime = -1;  // Réinitialiser le meilleur temps (facultatif si vous voulez le garder)

        // Afficher l'écran d'accueil
        ShowIntroScreen();
        while (true) {
            Niveau::Level level = ShowLevelMenu();  // Afficher le menu de sélection du niveau

            // Créer une nouvelle instance de Game avec le niveau choisi
            Game game(level);
            game.Initialize();  // Initialiser le jeu avec ce niveau

            SetTargetFPS(60);  // Définir le nombre de FPS à 60 pour une expérience fluide
            bool retry = false;
            do {
                retry = game.Update();  // Mettre à jour l'état du jeu à chaque frame
            } while (!WindowShouldClose() && !retry);  // Continuer tant que la fenêtre est ouverte et qu'on ne veut pas quitter

            if (!retry) break;  // Si l'utilisateur ne veut pas recommencer, sortir de la boucle
        }
    }

    // Fonction pour dessiner l'objectif (fromage Jerry) dans le labyrinthe
    void DrawGoal(float scaleFactor, int offsetX, int offsetY) {
        // Calcul des dimensions de chaque cellule dans le labyrinthe
        int cellWidth = SCREEN_WIDTH / maze.GetGridWidth() * scaleFactor;
        int cellHeight = SCREEN_HEIGHT / maze.GetGridHeight() * scaleFactor;

        // Calcul de la position de l'objectif dans le labyrinthe
        Vector2 goalPosition = {
            offsetX + goal.x * cellWidth,
            offsetY + goal.y * cellHeight
        };

        // Dessiner l'objectif (fromage Jerry) à la position calculée
        DrawTextureEx(goalTexture, goalPosition, 0.0f, scaleFactor, WHITE);
    }

    // Fonction d'initialisation du jeu
    void Initialize() {
        // Initialiser les variables du jeu
        player.position = Position(0, 0);  // Placer le joueur au début du labyrinthe
        gameWon = false;  // Le jeu n'est pas gagné au début
        timer = 0;  // Réinitialiser le chronomètre
        changeTimer = 0;  // Réinitialiser le timer de régénération du labyrinthe

        // Mettre à jour la taille de la grille du joueur
        player.SetGridSize(maze.GetGridWidth(), maze.GetGridHeight());
    }

    // Fonction pour sauvegarder le meilleur temps dans un fichier
    void SaveBestTime() {
        std::ofstream outfile("best_time.txt");
        if (outfile.is_open()) {
            outfile << bestTime;  // Sauvegarder le meilleur temps
            outfile.close();
        }
    }

    // Fonction pour réinitialiser le jeu
    void ResetGame() {
        // Réinitialiser la position du joueur
        player.position = Position(0, 0);
        gameWon = false;  // Réinitialiser l'état de victoire
        timer = 0;  // Réinitialiser le chronomètre
        changeTimer = 0;  // Réinitialiser le timer de régénération
        maze.Regenerate(player.position);  // Régénérer le labyrinthe
        player.SetGridSize(maze.GetGridWidth(), maze.GetGridHeight());  // Mettre à jour la taille de la grille du joueur
    }
    bool Update() {
    if (gameWon) {
        // Vérifier si le joueur a gagné et si le temps actuel est meilleur que le meilleur temps
        if (bestTime < 0 || timer < bestTime) {
            bestTime = timer;
            SaveBestTime();  // Sauvegarder le meilleur temps
        }

        BeginDrawing();
        ClearBackground(BLACK);  // Effacer l'écran avec un fond noir

        // Dessiner l'image de fond
        DrawTexture(backgroundTexture, 0, 0, WHITE);

        // Couleur dynamique pour "YOU WIN!"
        float time = GetTime();  // Temps écoulé
        int red = (int)(sin(time * 2.0f) * 127 + 128);  // Variation de 0 à 255
        int green = (int)(sin(time * 2.0f + 2.0f) * 127 + 128);
        int blue = (int)(sin(time * 2.0f + 4.0f) * 127 + 128);
        Color dynamicColor = {red, green, blue, 255};  // Couleur dynamique

        // Dessiner "YOU WIN!" avec la couleur dynamique
        DrawText("YOU WIN!", SCREEN_WIDTH / 2 - MeasureText("YOU WIN!", 85) / 2, 100, 80, dynamicColor);

        // Afficher les scores au centre
        DrawText("Score Actuel :", SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 - 60, 20, BLACK);
        DrawText(TextFormat("%02d:%02d", (int)timer / 60, (int)timer % 60), SCREEN_WIDTH / 2 + 50, SCREEN_HEIGHT / 2 - 60, 20, BLACK);

        DrawText("Best Time :", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 20, 20, BLACK);
        DrawText(TextFormat("%02d:%02d", (int)bestTime / 60, (int)bestTime % 60), SCREEN_WIDTH / 2 + 30, SCREEN_HEIGHT / 2 - 20, 20, BLACK);

        int bestMinutes = (int)bestTime / 60;
        int bestSeconds = (int)bestTime % 60;

        // Obtenir la position de la souris
        Vector2 mousePoint = GetMousePosition();

        // Définir les couleurs par défaut pour les boutons
        Color retryButtonColor = WHITE;
        Color quitButtonColor = WHITE;
        Color retryTextColor = DARKGRAY;
        Color quitTextColor = DARKGRAY;

        // Vérifier si la souris est sur le bouton "Retry"
        if (CheckCollisionPointRec(mousePoint, retryButton)) {
            retryButtonColor = DARKGRAY;  // Couleur de fond lorsqu'on survole
            retryTextColor = WHITE;       // Couleur du texte lorsqu'on survole
        }

        // Vérifier si la souris est sur le bouton "Quit"
        if (CheckCollisionPointRec(mousePoint, quitButton)) {
            quitButtonColor = DARKGRAY;  // Couleur de fond lorsqu'on survole
            quitTextColor = WHITE;       // Couleur du texte lorsqu'on survole
        }

        // Calculer la largeur totale des boutons (y compris l'espace entre eux)
        int totalButtonWidth = retryButton.width + quitButton.width + 20; // 20 pour l'espace entre les boutons

        // Positionner les boutons horizontalement au centre de l'écran
        retryButton.x = SCREEN_WIDTH / 2 - totalButtonWidth / 2;  // Déplacer "Retry" à gauche
        quitButton.x = retryButton.x + retryButton.width + 20;   // Déplacer "Quit" à droite du bouton "Retry"

        // Garder la même position verticale
        retryButton.y = quitButton.y = SCREEN_HEIGHT / 2 + 60;

        // Dessiner le bouton "Retry" avec les couleurs mises à jour
        DrawRectangleRec(retryButton, retryButtonColor);
        int retryTextWidth = MeasureText("Retry", 20);
        int retryTextX = retryButton.x + (retryButton.width - retryTextWidth) / 2;
        int retryTextY = retryButton.y + (retryButton.height - 20) / 2;
        DrawText("Retry", retryTextX, retryTextY, 20, retryTextColor);

        // Dessiner le bouton "Quit" avec les couleurs mises à jour
        DrawRectangleRec(quitButton, quitButtonColor);
        int quitTextWidth = MeasureText("Quit", 20);
        int quitTextX = quitButton.x + (quitButton.width - quitTextWidth) / 2;
        int quitTextY = quitButton.y + (quitButton.height - 20) / 2;
        DrawText("Quit", quitTextX, quitTextY, 20, quitTextColor);

        // Vérifier si le joueur clique sur le bouton "Retry" ou "Quit"
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mousePoint = GetMousePosition();
            if (CheckCollisionPointRec(mousePoint, retryButton)) {
                return true;  // Redémarrer le jeu
            }
            if (CheckCollisionPointRec(mousePoint, quitButton)) {
                CloseWindow();  // Fermer la fenêtre et quitter le jeu
                return false;   // Quitter le jeu
            }
        }

        EndDrawing();
        return false;
    }

    // Si le niveau est dynamique et que le jeu n'est pas en pause, régénérer le labyrinthe
    if (niveau.isDynamic() && !isPaused) {
        changeTimer += GetFrameTime();
        if (changeTimer >= 3.0f) {
            maze.Regenerate(player.position);  // Régénérer le labyrinthe
            player.SetGridSize(maze.GetGridWidth(), maze.GetGridHeight());
            changeTimer = 0;
        }
    }

    float scaleFactor = 0.75f; // Facteur de mise à l'échelle
    int reducedWidth = scaleFactor * SCREEN_WIDTH;
    int reducedHeight = scaleFactor * SCREEN_HEIGHT;

    // Calculer les décalages pour centrer le labyrinthe
    int offsetX = (SCREEN_WIDTH - reducedWidth) / 2;
    int offsetY = (SCREEN_HEIGHT - reducedHeight) / 2;


    if (niveau.niveau == Niveau::MOYEN ) {
        if(!isPaused){
            maze.GetMovingObstacle().Move();  // Déplacer l'obstacle
            if (maze.GetMovingObstacle().CheckCollision(player.position)) {
                player.position = Position(0, 0);  // Réinitialiser la position du joueur
            }
        }
        maze.GetMovingObstacle().Draw(scaleFactor, offsetX, offsetY); // Dessiner l'obstacle
    }

    if(!isPaused){
        bool gameOver = false;
        // Gérer les mouvements du joueur avec les touches directionnelles
        if (IsKeyPressed(KEY_RIGHT)) player.Move(1, 0, maze, gameWon);
        if (IsKeyPressed(KEY_LEFT)) player.Move(-1, 0, maze, gameWon);
        if (IsKeyPressed(KEY_UP)) player.Move(0, -1, maze, gameWon);
        if (IsKeyPressed(KEY_DOWN)) player.Move(0, 1, maze, gameWon);
    }

    if(!isPaused)
        timer += GetFrameTime();  // Mettre à jour le timer

    // Dessiner le point d'arrivée
    DrawGoal(scaleFactor, offsetX, offsetY);

    int currentMinutes = (int)timer / 60;
    int currentSeconds = (int)timer % 60;

    // Calculer la position centrée pour le texte du timer
    int fontSize = 20;
    const char* timeText = TextFormat(" %02d:%02d", currentMinutes, currentSeconds);

    int textWidth = MeasureText(timeText, fontSize); // Largeur du texte
    int centerX = (SCREEN_WIDTH - textWidth) / 2;   // Position X centrée
    int posY = 30;                                  // Position Y

    BeginDrawing();
    ClearBackground(Color{240, 220, 190, 255});
    maze.DrawMaze(player.position);  // Dessiner le labyrinthe
    player.Draw(scaleFactor, offsetX, offsetY);  // Dessiner le joueur

    // Dessiner l'icône du timer
    int iconWidth = timerIcon.width;
    int iconHeight = timerIcon.height;
    int iconPosX = centerX - iconWidth + 15;  // Position X de l'icône
    int iconPosY = posY - 20;  // Position Y de l'icône

    DrawTexture(timerIcon, iconPosX, iconPosY, WHITE);  // Dessiner l'icône du timer

    // Afficher le texte du temps
    DrawText(timeText, centerX, posY, fontSize, RED);  // Afficher le temps

    // Vérifier si le joueur clique sur le bouton pause/reprise
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), pauseButton)) {
        isPaused = !isPaused;  // Alterner entre pause et reprise
    }

    // Si le jeu est en pause, afficher l'écran de pause
    if (isPaused) {
        BeginDrawing();
        ClearBackground(Color{240, 220, 190, 255});
        DrawTexture(resumeTexture, pauseButton.x, pauseButton.y, WHITE);  // Afficher l'icône de reprise
        DrawText("Game Paused", SCREEN_WIDTH / 2 - MeasureText("Game Paused", 30) / 2, SCREEN_HEIGHT / 2 - 20, 30, BLACK);  // Afficher le texte "Game Paused"
        EndDrawing();
        return false;  // Ne pas mettre à jour le jeu si en pause
    }

    // Dessiner les boutons de contrôle
    BeginDrawing();
    ClearBackground(Color{240, 220, 190, 255});
    DrawTexture(pauseTexture, pauseButton.x, pauseButton.y, WHITE);  // Afficher le bouton pause

    if (!gameWon) {
        // Afficher les boutons Reset et Home
        DrawTexturePro(resetButtonTexture, {0, 0, (float)resetButtonTexture.width, (float)resetButtonTexture.height},
            {resetButton.x, resetButton.y, resetButton.width, resetButton.height}, {0, 0}, 0.0f, WHITE);

        DrawTexturePro(homeButtonTexture, {0, 0, (float)homeButtonTexture.width, (float)homeButtonTexture.height},
            {homeButton.x, homeButton.y, homeButton.width, homeButton.height}, {0, 0}, 0.0f, WHITE);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mousePoint = GetMousePosition();
            if (CheckCollisionPointRec(mousePoint, resetButton)) {
                ResetGame();  // Réinitialiser le jeu
            }
            if (CheckCollisionPointRec(mousePoint, homeButton)) {
                GoToIntroScreen();  // Réinitialiser tout et afficher l'écran d'accueil
            }
        }
    }

    // Afficher le texte du temps à côté de l'icône
    DrawText(timeText, centerX, posY, fontSize, RED);
    EndDrawing();
    return false;
}
};
int main() {
    // Initialiser la fenêtre du jeu avec les dimensions spécifiées
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Maze Game");

    // Afficher l'écran d'introduction (par exemple, un écran d'accueil ou de présentation)
    ShowIntroScreen();

    // Boucle principale du jeu
    while (true) {
        // Afficher le menu des niveaux et récupérer le niveau sélectionné
        Niveau::Level level = ShowLevelMenu();

        // Créer un objet de jeu en fonction du niveau sélectionné
        Game game(level);
        game.Initialize();  // Initialiser le jeu (par exemple, charger les ressources, etc.)

        // Définir la fréquence de mise à jour de l'écran (ici, 60 FPS)
        SetTargetFPS(60);

        bool retry = false;  // Variable pour vérifier si l'utilisateur veut réessayer le jeu
        do {
            // Mettre à jour l'état du jeu (par exemple, déplacer le joueur, vérifier les collisions, etc.)
            retry = game.Update();
        } while (!WindowShouldClose() && !retry);  // Continuer tant que la fenêtre n'est pas fermée et que l'utilisateur ne veut pas réessayer

        // Si l'utilisateur ne veut pas réessayer, quitter la boucle du jeu
        if (!retry) break;
    }

    // Fermer le périphérique audio après la fin du jeu
    CloseAudioDevice();

    // Fermer la fenêtre du jeu
    CloseWindow();
    return 0;  // Terminer l'exécution du programme
}