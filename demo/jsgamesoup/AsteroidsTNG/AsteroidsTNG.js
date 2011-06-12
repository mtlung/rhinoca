var pi2 = Math.PI * 2;

function startAsteroidsTNG(gs) {
	/*** Define some different types of things ***/
	t_ship = 1;
	t_asteroid = 2;
	
	/*** reload the game ***/
	function doReload(secs) {
		setTimeout(function() {window.location.href = unescape(window.location.pathname);}, 1000 * secs);
	}
	
	/*** A single asteroid ***/
	function Asteroid(world, data, asteroidScale, quadrant) {
		this.type = t_asteroid;
		this.world = world;
		// get variables from the incoming data
		this.id = data.id
		this.x = data.x * gs.width + quadrant[0] * gs.width;
		this.y = data.y * gs.height + quadrant[1] * gs.height;
		this.angle = data.angle;
		this.radius = 100 * asteroidScale + 30;
		this.quadrant = quadrant;
		this.strokeStyle = 'rgba(255, 255, 255, 1.0)';
		this.fillStyle = 'rgba(115, 115, 115, 1.0)';
		var maxrad = 0;
		
		// structure of this shape
		this.points = [];
		for (p=0; p<data.points.length; p++) {
			var newpoint = [this.radius * data.points[p][0], this.radius * data.points[p][1]];
			this.points.push(newpoint);
			var newrad = Math.sqrt(Math.pow(newpoint[0], 2) + Math.pow(newpoint[1], 2));
			if (newrad > maxrad) {
				maxrad = newrad;
			}
		}
		this.poly = [];
		// precalculate rotated version
		for (n=0; n<data.points.length; n++)
			this.points[n] = [this.points[n][0] * Math.cos(this.angle) - this.points[n][1] * Math.sin(this.angle) + this.x, this.points[n][0] * Math.sin(this.angle) + this.points[n][1] * Math.cos(this.angle) + this.y];
		
		this.update = function() {
			// update our shape definition
			for (n=0; n<this.points.length; n++) {
				this.poly[n] = [this.points[n][0] - this.world.cameraX(), this.points[n][1] - this.world.cameraY()];
			}
		}
		
		this.draw = function(c) {
			c.strokeStyle = this.strokeStyle;
			c.fillStyle = this.fillStyle;
			gs.polygon(this.poly);
			//this.draw_circle(c);
		}
		
		this.headTowards = function(where) {
			this.heading = where;
		}
		
		this.get_collision_circle = function() {
			return [[this.x, this.y], maxrad];
		}
		
		this.get_collision_poly = function() {
			return this.poly;
		}
		
		this.draw_circle = function(c) {
			var bits = this.get_collision_circle();
			c.beginPath();
			c.arc(bits[0][0] - this.world.cameraX(), bits[0][1] - this.world.cameraY(), bits[1], 0, pi2, false);
			//c.arc(100, 100, 50, 0, pi2, false);
			c.closePath();
			c.stroke();
		}
	}
	
	/*** A background parallax star ***/
	function Star(world) {
		this.world = world;
		this.rate = gs.random(0.5, 1.0);
		this.size = Math.round(gs.random(0, 3));
		this.x = gs.random(0, 10000);
		this.y = gs.random(0, 10000);
		this.fs = 'rgba(255, 255, 255, ' + (this.rate - 0.2) + ')';
		
		this.update = function() {
		}
		
		this.getX = function() {
			return Math.round((this.x - this.world.cameraX()) * this.rate % gs.width);
		}
		
		this.getY = function() {
			return Math.round((this.y - this.world.cameraY()) * this.rate % gs.height);
		}
		
		if (this.size > 1.0) {
			this.draw = function(c) {
				c.strokeStyle = this.fs;
				c.beginPath();
				c.arc(this.getX(), this.getY(), this.size, 0, Math.PI*2, true);
				c.closePath();
				c.stroke();
			}
		} else {
			this.draw = function(c) {
				c.fillStyle = this.fs;
				var sx = this.getX() - 0.5;
				var sy = this.getY() - 0.5;
				c.beginPath();
				c.rect(sx, sy, 1, 1);
				for (var i=0; i<2; i++) {
					for (var j=0; j<2; j++) {
						c.rect(sx + (i * 2 - 1) * 2, sy + (j * 2 - 1) * 2, 1, 1);
						c.rect(sx + (i * 2 - 1), sy + (j * 2 - 1), 1, 1);
					}
				}
				c.closePath();
				c.fill();
				
			}
		}
	}
	
	/*** A player ship ***/
	function Ship(world) {
		this.type = t_ship;
		this.world = world;
		this.x = gs.width / 2;
		this.y = gs.height / 2;
		this.angle = 0;
		this.speed = 0;
		this.turnRate = 0.1;
		this.accel = 0.3;
		this.radius = 13;
		this.points = [[0, -this.radius], [-7, 7], [7, 7]];
		this.poly = [];
		this.lastsmoke = null;
		this.world.setPlayer(this);
		this.strokeStyle = 'rgba(255, 255, 255, 1.0)';
		this.fillStyle = 'rgba(115, 115, 115, 1.0)';
		this.followPointer = false;
		this.heading = null;
		this.priority = 10;
		
		this.keyHeld_37 = this.keyDown_37 = function () {
			this.incAngle(-1);
		}
		
		this.keyHeld_39 = this.keyDown_39 = function () {
			this.incAngle(1);
		}
		
		this.keyDown_38 = function () {
			this.speed = 1;
		}
		
		this.keyHeld_38 = function () {
			if (this.speed < 3.0)
				this.speed += this.accel;
		}
		
		this.keyDown_32 = function () {
			// pass
		}
		
		this.keyDown = function (keyCode) {
			//console.log(keyCode);
		}
		
		/*this.collided = function(other) {
			if (other.type == asteroid) {
				this.explode();
				other.explode();
				doReload(1);
			}
		}*/
		
		this.incAngle = function(sign) {
			this.angle = (this.angle + sign * this.turnRate) % (2 * Math.PI);
		}
		
		this.explode = function() {
			gs.delEntity(this);
		}
		
		this.setFollowPointer = function(headingpoint) {
			this.followPointer = true;
		}
		
		this.stopFollowPointer = function() {
			this.followPointer = false;
		}
		
		this.get_collision_circle = function() {
			return [[this.x, this.y], this.radius];
		}
		
		this.collide_circle = function(who) {
			var polycollision = collide.collide_poly_entities(this, who);
			if (polycollision) {
				var collisionpoint = polycollision[1];
				if (collisionpoint) {
					//var bouncevector = [(collisionpoint[0] - gs.width / 2) - ((this.x - this.world.cameraX()) - gs.width / 2), (collisionpoint[1] - gs.height / 2) - ((this.y - this.world.cameraY()) - gs.height / 2)];
					var p1 = collisionpoint[0];
					var p2 = collisionpoint[1];
					var bouncevector = [p2[1] - p1[1], p1[0] - p2[0]];
					var bouncesize = Math.sqrt(Math.pow(bouncevector[0], 2) + Math.pow(bouncevector[1], 2));
					var bouncenormal = [bouncevector[0] / bouncesize, bouncevector[1] / bouncesize];
					// TODO: fix this up - going into a point doesn't work correctly
					if (this.bouncevector) {
						this.bouncevector[0] += bouncenormal[0];
						this.bouncevector[1] += bouncenormal[1];
						// renormalize
						var bouncesize = Math.sqrt(Math.pow(this.bouncevector[0], 2) + Math.pow(this.bouncevector[1], 2));
						this.bouncevector = [this.bouncevector[0] / bouncesize, this.bouncevector[1] / bouncesize];
					} else {
						this.bouncevector = bouncenormal;
					}
				} else {
					// TODO: bounce the player right out in the direction away from center of the asteroid
				}
				this.collided = true;
			}
		}
		
		this.get_collision_poly = function() {
			return this.poly;
		}
		
		this.draw_circle = function(c) {
			var bits = this.get_collision_circle();
			c.beginPath();
			c.arc(bits[0][0] - this.world.cameraX(), bits[0][1] - this.world.cameraY(), bits[1], 0, pi2, false);
			c.closePath();
			c.stroke();
		}
		
		this.update = function() {
			// if we have a bouncevector, add it to our position
			if (this.bouncevector) {
				this.x -= this.bouncevector[0] * Math.max(this.speed, 0.1);
				this.y -= this.bouncevector[1] * Math.max(this.speed, 0.1);
				this.bouncevector = null;
				this.speed = 0;
			}
			// check if followpointer is on
			if (this.followPointer) {
				var heading = [
					(gs.pointerPosition[0] - gs.width / 2 + this.world.cameraX()) - (this.x - gs.width / 2),
					gs.pointerPosition[1] - gs.height / 2 + this.world.cameraY() - (this.y - gs.height / 2),
				];
				// rotate our heading
				var pts = [heading[0] * Math.cos(this.angle) + heading[1] * Math.sin(this.angle), heading[0] * Math.sin(this.angle) - heading[1] * Math.cos(this.angle)];
				this.heading = Math.atan2(pts[0], pts[1]);
				//console.log(angle);
				//this.heading = (this.angle - angle) % Math.PI;
				//console.log(this.heading);
				//this.heading = null;
			} else {
				this.heading = null;
			}
			// if the user is doing touch/mouse events then head towards the selected heading
			if (this.heading) {
				// turn and head towards it
				//console.log(this.heading);
				if (Math.abs(this.heading) < Math.PI / 2 && this.speed < 3.0) {
					this.speed += this.accel;
				}
				if (this.heading > 0.1) {
					this.incAngle(1);
				} else if (this.heading < -0.1) {
					this.incAngle(-1);
				}
			}
			// friction
			if (this.speed > 0.1)
				this.speed -= 0.1;
			else
				this.speed = 0;
			// update our position based on our angle and speed
			this.x = this.x + this.speed * Math.sin(this.angle);
			this.y = this.y - this.speed * Math.cos(this.angle);
			// get our newly translated polygon from angle
			for (n=0; n<this.points.length; n++) {
				this.poly[n] = [this.points[n][0] * Math.cos(this.angle) - this.points[n][1] * Math.sin(this.angle) + this.x - this.world.cameraX(), this.points[n][0] * Math.sin(this.angle) + this.points[n][1] * Math.cos(this.angle) + this.y - this.world.cameraY()];
			}
			// make smoke behind this ship
			if (this.speed && (!gs.inEntities(this.lastsmoke) || gs.distance([this.lastsmoke.x, this.lastsmoke.y], [this.x, this.y]) > 15)) {
				this.lastsmoke = new Smoke(world, this.x - 9 * Math.sin(this.angle), this.y + 9 * Math.cos(this.angle));
				gs.addEntity(this.lastsmoke);
			}
		}
		
		this.draw = function(c) {
			var poly = this.poly;
			c.strokeStyle = this.strokeStyle;
			c.fillStyle = this.fillStyle;
			c.beginPath();
			c.moveTo(poly[0][0], poly[0][1]);
			for (var n = 0; n < poly.length; n++) {
				c.lineTo(poly[n][0], poly[n][1]);
			}
			c.lineTo(poly[0][0], poly[0][1]);
			c.closePath();
			c.fill();
			c.stroke();
			
			if (this.collided)
				c.strokeStyle = 'rgb(255, 0, 0)';
			//this.draw_circle(c);
			this.collided = false;
		}
	}
	
	var smokeStrength = [];
	for (var r=0; r<10; r++) {
		smokeStrength[r] = 'rgba(200, 200, 200, ' + (r/10) + ')';
	}
	/*** Smoke coming out of the ship ***/
	function Smoke(world, x, y) {
		this.x = x;
		this.y = y;
		this.world = world;
		this.life = 1.0;
		
		this.draw = function(c) {
			c.strokeStyle = smokeStrength[Math.floor(this.life * 10)];
			c.beginPath();
			c.arc(this.x - this.world.cameraX(), this.y - this.world.cameraY(), 2, 0, pi2, true);
			c.closePath();
			c.stroke();
		}
		
		this.update = function() {
			this.life -= 0.08;
			if (this.life < 0)
			{
				gs.delEntity(this);
				this.life = 0.01;
			}
		}
	}
	
	/*** World ***/
	function World() {
		this.player = null;
		this.x = 0;
		this.y = 0;
		this.w = gs.width / 2;
		this.h = gs.height / 2;
		this.quadrant = [0, 0];
		// our procedural map generator with seedable deterministic random number generator
		var map = new Map(new SeedableRandom());
		this.relx = 0;
		this.rely = 0;
		
		this.setPlayer = function(player) {
			this.player = player;
			this.x = player.x;
			this.y = player.y;
			this.updateQuadrant();
		}
		
		this.updateQuadrant = function() {
			this.quadrant = [Math.floor(this.player.x / gs.width),
				Math.floor(this.player.y / gs.height)];
			this.getQuadrant(this.quadrant);
		}
		
		this.cameraX = function() {
			return this.relx;
		}
		
		this.cameraY = function () {
			return this.rely;
		}
		
		this.draw = function() {
			gs.clear();
			gs.background('rgba(100, 100, 100, 1.0)');
		}
		
		this.update = function() {
			this.x = this.x + (this.player.x - this.x) * 0.1;
			this.y = this.y + (this.player.y - this.y) * 0.1;
			this.relx = this.x - this.w;
			this.rely = this.y - this.h;
			if (Math.floor(this.player.x / gs.width) != this.quadrant[0] ||
				Math.floor(this.player.y / gs.height) != this.quadrant[1]) {
				this.updateQuadrant();
			}
			// do any collisions
			if (this.player) {
				collide.circles([this.player], asteroidcache);
			}
		}
		
		// cache of all asteroid objects by their ID
		var asteroidcache = {};
		var asteroidcachesize = 0;
		
		// let us get every asteroid around quadrant [x, y] using our map-generator object
		this.getQuadrant = function(quadrant) {
			var allasteroids = [];
			for (var i=-1; i<2; i++) {
				for (var j=-1; j<2; j++) {
					var pos = [quadrant[0] + i, quadrant[1] + j];
					var quadrantData = map.getQuadrantData(pos);
					var asteroids = quadrantData['asteroids'];
					allasteroids = allasteroids.concat(asteroids);
					for (var a=0; a<asteroids.length; a++) {
						if (!asteroidcache[asteroids[a]]) {
							asteroidcache[asteroids[a]] = new Asteroid(w, map.getAsteroidData(asteroids[a], quadrantData['asteroidSize']), quadrantData['asteroidSize'], pos);
							asteroidcachesize += 1;
							gs.addEntity(asteroidcache[asteroids[a]]);
						}
					}
				}
			}
			// get rid of the asteroids in the cache which we no longer care about
			for (a in asteroidcache) {
				if (allasteroids.indexOf(asteroidcache[a].id) == -1) {
					gs.delEntity(asteroidcache[a])
					delete asteroidcache[a];
					asteroidcachesize -= 1;
				}
			}
		}
		
		// use touch/mouse to guide the ship
		this.pointerDown = function() {
			if (this.player) {
				this.player.setFollowPointer();
			}
		}
		
		this.pointerUp = function() {
			if (this.player) {
				this.player.stopFollowPointer();
			}
		}
		
		this.pointerBox = function() {
			return [0, 0, gs.width, gs.height];
		}
	}
	
	w = new World();
	gs.addEntity(w);
	for (n=0; n<10; n++) {
		gs.addEntity(new Star(w));
	}
	gs.addEntity(new Ship(w));
}
